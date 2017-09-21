// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <activation.h>
#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_template_builder.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/message_center/notification.h"

namespace mswr = Microsoft::WRL;
namespace mswrw = Microsoft::WRL::Wrappers;

namespace winfoundtn = ABI::Windows::Foundation;
namespace winui = ABI::Windows::UI;
namespace winxml = ABI::Windows::Data::Xml;

using message_center::RichNotificationData;

namespace {

// Provides access to functions in combase.dll which may not be available on
// Windows 7. Loads functions dynamically at runtime to prevent library
// dependencies.
// TODO(finnur): Move this to base/win.
class CombaseFunctions {
 public:
  static bool LoadFunctions() {
    HMODULE combase_dll = ::LoadLibrary(L"combase.dll");
    if (!combase_dll)
      return false;

    get_factory_func_ = reinterpret_cast<decltype(&::RoGetActivationFactory)>(
        ::GetProcAddress(combase_dll, "RoGetActivationFactory"));
    if (!get_factory_func_)
      return false;

    activate_instance_func_ = reinterpret_cast<decltype(&::RoActivateInstance)>(
        ::GetProcAddress(combase_dll, "RoActivateInstance"));
    if (!activate_instance_func_)
      return false;

    create_string_func_ = reinterpret_cast<decltype(&::WindowsCreateString)>(
        ::GetProcAddress(combase_dll, "WindowsCreateString"));
    if (!create_string_func_)
      return false;

    delete_string_func_ = reinterpret_cast<decltype(&::WindowsDeleteString)>(
        ::GetProcAddress(combase_dll, "WindowsDeleteString"));
    if (!delete_string_func_)
      return false;

    create_string_ref_func_ =
        reinterpret_cast<decltype(&::WindowsCreateStringReference)>(
            ::GetProcAddress(combase_dll, "WindowsCreateStringReference"));
    if (!create_string_ref_func_)
      return false;

    return true;
  }

  static HRESULT RoGetActivationFactory(HSTRING class_id,
                                        const IID& iid,
                                        void** out_factory) {
    DCHECK(get_factory_func_);
    return get_factory_func_(class_id, iid, out_factory);
  }

  static HRESULT RoActivateInstance(HSTRING class_id, IInspectable** instance) {
    DCHECK(activate_instance_func_);
    return activate_instance_func_(class_id, instance);
  }

  static HRESULT WindowsCreateString(const base::char16* src,
                                     uint32_t len,
                                     HSTRING* out_hstr) {
    DCHECK(create_string_func_);
    return create_string_func_(src, len, out_hstr);
  }

  static HRESULT WindowsDeleteString(HSTRING hstr) {
    DCHECK(delete_string_func_);
    return delete_string_func_(hstr);
  }

  static HRESULT WindowsCreateStringReference(const base::char16* src,
                                              uint32_t len,
                                              HSTRING_HEADER* out_header,
                                              HSTRING* out_hstr) {
    DCHECK(create_string_ref_func_);
    return create_string_ref_func_(src, len, out_header, out_hstr);
  }

 private:
  CombaseFunctions() = default;
  ~CombaseFunctions() = default;

  static decltype(&::RoGetActivationFactory) get_factory_func_;
  static decltype(&::RoActivateInstance) activate_instance_func_;
  static decltype(&::WindowsCreateString) create_string_func_;
  static decltype(&::WindowsDeleteString) delete_string_func_;
  static decltype(&::WindowsCreateStringReference) create_string_ref_func_;
};

decltype(&::RoGetActivationFactory) CombaseFunctions::get_factory_func_ =
    nullptr;
decltype(&::RoActivateInstance) CombaseFunctions::activate_instance_func_ =
    nullptr;
decltype(&::WindowsCreateString) CombaseFunctions::create_string_func_ =
    nullptr;
decltype(&::WindowsDeleteString) CombaseFunctions::delete_string_func_ =
    nullptr;
decltype(
    &::WindowsCreateStringReference) CombaseFunctions::create_string_ref_func_ =
    nullptr;

// Scoped HSTRING class to maintain lifetime of HSTRINGs allocated with
// WindowsCreateString().
// TODO(finnur): Move this to base/win.
class ScopedHStringTraits {
 public:
  static HSTRING InvalidValue() { return nullptr; }

  static void Free(HSTRING hstr) {
    CombaseFunctions::WindowsDeleteString(hstr);
  }
};

// TODO(finnur): Move this to base/win.
class ScopedHString : public base::ScopedGeneric<HSTRING, ScopedHStringTraits> {
 public:
  explicit ScopedHString(const base::char16* str) : ScopedGeneric(nullptr) {
    HSTRING hstr;
    HRESULT hr = CombaseFunctions::WindowsCreateString(
        str, static_cast<uint32_t>(wcslen(str)), &hstr);
    if (FAILED(hr))
      VLOG(1) << "WindowsCreateString failed";
    else
      reset(hstr);
  }
};

// A convenience class for creating HStringReferences.
class HStringRef {
 public:
  explicit HStringRef(const base::char16* str) {
    HRESULT hr = CombaseFunctions::WindowsCreateStringReference(
        str, static_cast<uint32_t>(wcslen(str)), &header_, &hstr_);
    if (FAILED(hr))
      VLOG(1) << "WindowsCreateStringReference failed";
  }

  HSTRING Get() { return hstr_; }

 private:
  HSTRING hstr_;
  HSTRING_HEADER header_;

  DISALLOW_COPY_AND_ASSIGN(HStringRef);
};

// Templated wrapper for winfoundtn::GetActivationFactory().
template <unsigned int size, typename T>
HRESULT CreateActivationFactory(wchar_t const (&class_name)[size], T** object) {
  HStringRef ref_class_name(class_name);
  return CombaseFunctions::RoGetActivationFactory(ref_class_name.Get(),
                                                  IID_PPV_ARGS(object));
}

// TODO(finnur): Make this a member function of the class and unit test it.
// Obtain an IToastNotification interface from a given XML (provided by the
// NotificationTemplateBuilder).
HRESULT GetToastNotification(
    const NotificationTemplateBuilder& notification_template_builder,
    winui::Notifications::IToastNotification** toast_notification) {
  *toast_notification = nullptr;

  HStringRef ref_class_name(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument);
  mswr::ComPtr<IInspectable> inspectable;
  HRESULT hr =
      CombaseFunctions::RoActivateInstance(ref_class_name.Get(), &inspectable);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to activate the XML Document";
    return hr;
  }

  mswr::ComPtr<winxml::Dom::IXmlDocumentIO> document_io;
  hr = inspectable.As<winxml::Dom::IXmlDocumentIO>(&document_io);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get XmlDocument as IXmlDocumentIO";
    return hr;
  }

  base::string16 notification_template =
      notification_template_builder.GetNotificationTemplate();
  HStringRef ref_template(notification_template.c_str());
  hr = document_io->LoadXml(ref_template.Get());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to load the template's XML into the document";
    return hr;
  }

  mswr::ComPtr<winxml::Dom::IXmlDocument> document;
  hr = document_io.As<winxml::Dom::IXmlDocument>(&document);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to get as XMLDocument";
    return hr;
  }

  mswr::ComPtr<winui::Notifications::IToastNotificationFactory>
      toast_notification_factory;
  hr = CreateActivationFactory(
      RuntimeClass_Windows_UI_Notifications_ToastNotification,
      toast_notification_factory.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create the IToastNotificationFactory";
    return hr;
  }

  hr = toast_notification_factory->CreateToastNotification(document.Get(),
                                                           toast_notification);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create the IToastNotification";
    return hr;
  }

  return S_OK;
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeWin();
}

NotificationPlatformBridgeWin::NotificationPlatformBridgeWin() {
  com_functions_initialized_ = CombaseFunctions::LoadFunctions();
}

NotificationPlatformBridgeWin::~NotificationPlatformBridgeWin() = default;

void NotificationPlatformBridgeWin::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // TODO(finnur): Move this to a RoInitialized thread, as per crbug.com/761039.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
      toast_manager;
  HRESULT hr = CreateActivationFactory(
      RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
      toast_manager.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create the ToastNotificationManager";
    return;
  }

  base::string16 browser_model_id =
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall());
  ScopedHString application_id(browser_model_id.c_str());
  mswr::ComPtr<winui::Notifications::IToastNotifier> notifier;
  hr =
      toast_manager->CreateToastNotifierWithId(application_id.get(), &notifier);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create the ToastNotifier";
    return;
  }

  std::unique_ptr<NotificationTemplateBuilder> notification_template =
      NotificationTemplateBuilder::Build(notification_id, notification);
  mswr::ComPtr<winui::Notifications::IToastNotification> toast;
  hr = GetToastNotification(*notification_template, &toast);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to get a toast notification";
    return;
  }

  hr = notifier->Show(toast.Get());
  if (FAILED(hr))
    LOG(ERROR) << "Unable to display the notification";
}

void NotificationPlatformBridgeWin::Close(const std::string& profile_id,
                                          const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(peter): Implement the ability to close notifications.
}

void NotificationPlatformBridgeWin::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const GetDisplayedNotificationsCallback& callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto displayed_notifications = std::make_unique<std::set<std::string>>();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&displayed_notifications),
                 false /* supports_synchronization */));
}

void NotificationPlatformBridgeWin::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(com_functions_initialized_);
}
