// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/usb_chooser_dialog_android.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/common/url_constants.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/base/device_client.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/cpp/filter_utils.h"
#include "device/usb/usb_device.h"
#include "device/usb/webusb_descriptors.h"
#include "device/vr/features/features.h"
#include "jni/UsbChooserDialog_jni.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_VR)
#include "chrome/browser/android/vr_shell/vr_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_VR)

using device::UsbDevice;

namespace {

void OnDevicePermissionRequestComplete(
    scoped_refptr<UsbDevice> device,
    const device::mojom::UsbChooserService::GetPermissionCallback& callback,
    bool granted) {
  device::mojom::UsbDeviceInfoPtr device_info;
  if (granted)
    device_info = device::mojom::UsbDeviceInfo::From(*device);
  callback.Run(std::move(device_info));
}

}  // namespace

UsbChooserDialogAndroid::UsbChooserDialogAndroid(
    std::vector<device::mojom::UsbDeviceFilterPtr> filters,
    content::RenderFrameHost* render_frame_host,
    const device::mojom::UsbChooserService::GetPermissionCallback& callback)
    : render_frame_host_(render_frame_host),
      callback_(callback),
      usb_service_observer_(this),
      filters_(std::move(filters)),
      weak_factory_(this) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
#if BUILDFLAG(ENABLE_VR)
  if (vr_shell::VrTabHelper::IsInVr(web_contents)) {
    DCHECK(!callback_.is_null());
    callback_.Run(nullptr);
    callback_.Reset();  // Reset |callback_| so that it is only run once.
    return;
  }
#endif
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  if (!usb_service)
    return;

  if (!usb_service_observer_.IsObserving(usb_service))
    usb_service_observer_.Add(usb_service);

  // Create (and show) the UsbChooser dialog.
  base::android::ScopedJavaLocalRef<jobject> window_android =
      content::ContentViewCore::FromWebContents(web_contents)
          ->GetWindowAndroid()
          ->GetJavaObject();
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> origin_string =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatUrlForSecurityDisplay(GURL(
                   render_frame_host->GetLastCommittedOrigin().Serialize())));
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  java_dialog_.Reset(Java_UsbChooserDialog_create(
      env, window_android, origin_string, security_info.security_level,
      reinterpret_cast<intptr_t>(this)));

  if (!java_dialog_.is_null()) {
    usb_service->GetDevices(
        base::Bind(&UsbChooserDialogAndroid::GotUsbDeviceList,
                   weak_factory_.GetWeakPtr()));
  }
}

UsbChooserDialogAndroid::~UsbChooserDialogAndroid() {
  if (!callback_.is_null())
    callback_.Run(nullptr);

  if (!java_dialog_.is_null()) {
    Java_UsbChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                      java_dialog_);
  }
}

void UsbChooserDialogAndroid::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  if (DisplayDevice(device)) {
    AddDeviceToChooserDialog(device);
    devices_.push_back(device);
  }
}

void UsbChooserDialogAndroid::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  auto it = std::find(devices_.begin(), devices_.end(), device);
  if (it != devices_.end()) {
    RemoveDeviceFromChooserDialog(device);
    devices_.erase(it);
  }
}

void UsbChooserDialogAndroid::Select(const std::string& guid) {
  for (size_t i = 0; i < devices_.size(); ++i) {
    scoped_refptr<UsbDevice>& device = devices_[i];
    if (device->guid() == guid) {
      content::WebContents* web_contents =
          content::WebContents::FromRenderFrameHost(render_frame_host_);
      GURL embedding_origin =
          web_contents->GetMainFrame()->GetLastCommittedURL().GetOrigin();
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      UsbChooserContext* chooser_context =
          UsbChooserContextFactory::GetForProfile(profile);
      chooser_context->GrantDevicePermission(
          render_frame_host_->GetLastCommittedURL().GetOrigin(),
          embedding_origin, device->guid());

      device->RequestPermission(
          base::Bind(&OnDevicePermissionRequestComplete, device, callback_));
      callback_.Reset();  // Reset |callback_| so that it is only run once.

      Java_UsbChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                        java_dialog_);
      RecordWebUsbChooserClosure(
          device->serial_number().empty()
              ? WEBUSB_CHOOSER_CLOSED_EPHEMERAL_PERMISSION_GRANTED
              : WEBUSB_CHOOSER_CLOSED_PERMISSION_GRANTED);
      return;
    }
  }
}

void UsbChooserDialogAndroid::Cancel() {
  DCHECK(!callback_.is_null());
  callback_.Run(nullptr);
  callback_.Reset();  // Reset |callback_| so that it is only run once.
  Java_UsbChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                    java_dialog_);

  RecordWebUsbChooserClosure(devices_.size() == 0
                                 ? WEBUSB_CHOOSER_CLOSED_CANCELLED_NO_DEVICES
                                 : WEBUSB_CHOOSER_CLOSED_CANCELLED);
}

void UsbChooserDialogAndroid::OnItemSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& device_id) {
  Select(base::android::ConvertJavaStringToUTF8(env, device_id));
}

void UsbChooserDialogAndroid::OnDialogCancelled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  Cancel();
}

void UsbChooserDialogAndroid::LoadUsbHelpPage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  OpenUrl(chrome::kChooserUsbOverviewURL);
  Cancel();
}

// Get a list of devices that can be shown in the chooser bubble UI for
// user to grant permsssion.
void UsbChooserDialogAndroid::GotUsbDeviceList(
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  for (const auto& device : devices) {
    if (DisplayDevice(device)) {
      AddDeviceToChooserDialog(device);
      devices_.push_back(device);
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UsbChooserDialog_setIdleState(env, java_dialog_);
}

void UsbChooserDialogAndroid::AddDeviceToChooserDialog(
    scoped_refptr<UsbDevice> device) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> device_guid =
      base::android::ConvertUTF8ToJavaString(env, device->guid());
  base::android::ScopedJavaLocalRef<jstring> device_name =
      base::android::ConvertUTF16ToJavaString(env, device->product_string());
  Java_UsbChooserDialog_addDevice(env, java_dialog_, device_guid, device_name);
}

void UsbChooserDialogAndroid::RemoveDeviceFromChooserDialog(
    scoped_refptr<UsbDevice> device) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> device_guid =
      base::android::ConvertUTF8ToJavaString(env, device->guid());
  Java_UsbChooserDialog_removeDevice(env, java_dialog_, device_guid);
}

void UsbChooserDialogAndroid::OpenUrl(const std::string& url) {
  content::WebContents::FromRenderFrameHost(render_frame_host_)
      ->OpenURL(
          content::OpenURLParams(GURL(url), content::Referrer(),
                                 WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                 false));  // is_renderer_initiated
}

bool UsbChooserDialogAndroid::DisplayDevice(
    scoped_refptr<UsbDevice> device) const {
  if (!UsbDeviceFilterMatchesAny(filters_, *device))
    return false;

  if (UsbBlocklist::Get().IsExcluded(device))
    return false;

  return true;
}

// static
bool UsbChooserDialogAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
