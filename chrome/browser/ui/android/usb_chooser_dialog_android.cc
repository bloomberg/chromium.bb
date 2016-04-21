// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/usb_chooser_dialog_android.h"

#include <stddef.h>

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/common/url_constants.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/core/device_client.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/webusb_descriptors.h"
#include "jni/UsbChooserDialog_jni.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

UsbChooserDialogAndroid::UsbChooserDialogAndroid(
    mojo::Array<device::usb::DeviceFilterPtr> device_filters,
    content::RenderFrameHost* render_frame_host,
    const device::usb::ChooserService::GetPermissionCallback& callback)
    : render_frame_host_(render_frame_host),
      callback_(callback),
      usb_service_observer_(this),
      weak_factory_(this) {
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  if (!usb_service)
    return;

  if (!usb_service_observer_.IsObserving(usb_service))
    usb_service_observer_.Add(usb_service);

  if (!device_filters.is_null())
    filters_ = device_filters.To<std::vector<device::UsbDeviceFilter>>();

  // Create (and show) the UsbChooser dialog.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  base::android::ScopedJavaLocalRef<jobject> window_android =
      content::ContentViewCore::FromWebContents(web_contents)
          ->GetWindowAndroid()
          ->GetJavaObject();
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> origin_string =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatUrlForSecurityDisplay(
                   render_frame_host->GetLastCommittedURL()));
  ChromeSecurityStateModelClient* security_model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);
  DCHECK(security_model_client);
  java_dialog_.Reset(Java_UsbChooserDialog_create(
      env, window_android.obj(), origin_string.obj(),
      security_model_client->GetSecurityInfo().security_level,
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
                                      java_dialog_.obj());
  }
}

void UsbChooserDialogAndroid::OnDeviceAdded(
    scoped_refptr<device::UsbDevice> device) {
  if (device::UsbDeviceFilter::MatchesAny(device, filters_) &&
      FindInWebUsbAllowedOrigins(
          device->webusb_allowed_origins(),
          render_frame_host_->GetLastCommittedURL().GetOrigin())) {
    AddDeviceToChooserDialog(device);
    devices_.push_back(device);
  }
}

void UsbChooserDialogAndroid::OnDeviceRemoved(
    scoped_refptr<device::UsbDevice> device) {
  auto it = std::find(devices_.begin(), devices_.end(), device);
  if (it != devices_.end()) {
    RemoveDeviceFromChooserDialog(device);
    devices_.erase(it);
  }
}

void UsbChooserDialogAndroid::Select(const std::string& guid) {
  for (size_t i = 0; i < devices_.size(); ++i) {
    if (devices_[i]->guid() == guid) {
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
          embedding_origin, devices_[i]->guid());
      device::usb::DeviceInfoPtr device_info_ptr =
          device::usb::DeviceInfo::From(*devices_[i]);
      callback_.Run(std::move(device_info_ptr));
      callback_.reset();  // Reset |callback_| so that it is only run once.
      Java_UsbChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                        java_dialog_.obj());

      RecordWebUsbChooserClosure(
          devices_[i]->serial_number().empty()
              ? WEBUSB_CHOOSER_CLOSED_EPHEMERAL_PERMISSION_GRANTED
              : WEBUSB_CHOOSER_CLOSED_PERMISSION_GRANTED);

      return;
    }
  }
}

void UsbChooserDialogAndroid::Cancel() {
  callback_.Run(nullptr);
  callback_.reset();  // Reset |callback_| so that it is only run once.
  Java_UsbChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                    java_dialog_.obj());

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
    const std::vector<scoped_refptr<device::UsbDevice>>& devices) {
  for (const auto& device : devices) {
    if (device::UsbDeviceFilter::MatchesAny(device, filters_) &&
        FindInWebUsbAllowedOrigins(
            device->webusb_allowed_origins(),
            render_frame_host_->GetLastCommittedURL().GetOrigin())) {
      AddDeviceToChooserDialog(device);
      devices_.push_back(device);
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UsbChooserDialog_setIdleState(env, java_dialog_.obj());
}

void UsbChooserDialogAndroid::AddDeviceToChooserDialog(
    scoped_refptr<device::UsbDevice> device) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> device_guid =
      base::android::ConvertUTF8ToJavaString(env, device->guid());
  base::android::ScopedJavaLocalRef<jstring> device_name =
      base::android::ConvertUTF16ToJavaString(env, device->product_string());
  Java_UsbChooserDialog_addDevice(env, java_dialog_.obj(), device_guid.obj(),
                                  device_name.obj());
}

void UsbChooserDialogAndroid::RemoveDeviceFromChooserDialog(
    scoped_refptr<device::UsbDevice> device) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> device_guid =
      base::android::ConvertUTF8ToJavaString(env, device->guid());
  base::android::ScopedJavaLocalRef<jstring> device_name =
      base::android::ConvertUTF16ToJavaString(env, device->product_string());
  Java_UsbChooserDialog_removeDevice(env, java_dialog_.obj(), device_guid.obj(),
                                     device_name.obj());
}

void UsbChooserDialogAndroid::OpenUrl(const std::string& url) {
  content::WebContents::FromRenderFrameHost(render_frame_host_)
      ->OpenURL(content::OpenURLParams(GURL(url), content::Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                       false));  // is_renderer_initiated
}

// static
bool UsbChooserDialogAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
