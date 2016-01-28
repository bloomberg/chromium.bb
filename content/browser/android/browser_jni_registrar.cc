// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/background_sync_network_observer_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/android/composited_touch_handle_drawable.h"
#include "content/browser/android/content_video_view.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_render_view.h"
#include "content/browser/android/content_view_statics.h"
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/download_controller_android_impl.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/popup_touch_handle_drawable.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/android/web_contents_observer_proxy.h"
#include "content/browser/device_sensors/sensor_manager_android.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/gamepad/gamepad_platform_data_fetcher_android.h"
#include "content/browser/geolocation/location_api_adapter_android.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/media/android/media_session.h"
#include "content/browser/mojo/service_registrar_android.h"
#include "content/browser/mojo/service_registry_android.h"
#include "content/browser/power_save_blocker_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"
#include "content/browser/speech/speech_recognizer_impl_android.h"
#include "content/browser/time_zone_monitor_android.h"
#include "content/browser/vr/android/cardboard/cardboard_vr_device.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "mojo/android/system/core_impl.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
    {"BrowserStartupController", content::RegisterBrowserStartupController},
    {"ChildProcessLauncher", content::RegisterChildProcessLauncher},
    {"ContentVideoView", content::ContentVideoView::RegisterContentVideoView},
    {"CoreImpl", mojo::android::RegisterCoreImpl},
    {"MediaResourceGetterImpl",
     content::MediaResourceGetterImpl::RegisterMediaResourceGetter},
    {"MediaSession", content::MediaSession::RegisterMediaSession},
    {"AndroidLocationApiAdapter",
     content::AndroidLocationApiAdapter::RegisterGeolocationService},
    {"BackgroundSyncNetworkObserverAndroid",
     content::BackgroundSyncNetworkObserverAndroid::Observer::
         RegisterNetworkObserver},
    {"BrowserAccessibilityManager",
     content::RegisterBrowserAccessibilityManager},
#if defined(ENABLE_WEBVR)
    {"CardboardVRDevice",
     content::CardboardVRDevice::RegisterCardboardVRDevice},
#endif
    {"ContentViewCore", content::RegisterContentViewCore},
    {"ContentViewRenderView",
     content::ContentViewRenderView::RegisterContentViewRenderView},
    {"DateTimePickerAndroid", content::RegisterDateTimeChooserAndroid},
    {"DownloadControllerAndroidImpl",
     content::DownloadControllerAndroidImpl::RegisterDownloadController},
    {"GamepadList",
     content::GamepadPlatformDataFetcherAndroid::
         RegisterGamepadPlatformDataFetcherAndroid},
    {"HandleViewResources",
     content::CompositedTouchHandleDrawable::RegisterHandleViewResources},
    {"InterstitialPageDelegateAndroid",
     content::InterstitialPageDelegateAndroid::
         RegisterInterstitialPageDelegateAndroid},
    {"LoadUrlParams", content::RegisterLoadUrlParams},
    {"MotionEventSynthesizer",
     content::SyntheticGestureTargetAndroid::RegisterMotionEventSynthesizer},
    {"NavigationControllerAndroid",
     content::NavigationControllerAndroid::Register},
    {"PopupTouchHandleDrawable",
     content::PopupTouchHandleDrawable::RegisterPopupTouchHandleDrawable},
    {"PowerSaveBlock", content::RegisterPowerSaveBlocker},
    {"RegisterImeAdapter", content::RegisterImeAdapter},
    {"ScreenOrientationProvider",
     content::ScreenOrientationDelegateAndroid::Register},
    {"SensorManagerAndroid", content::SensorManagerAndroid::Register},
    {"ServiceRegistrarAndroid", content::ServiceRegistrarAndroid::Register},
    {"ServiceRegistryAndroid", content::ServiceRegistryAndroid::Register},
    {"SpeechRecognizerImplAndroid",
     content::SpeechRecognizerImplAndroid::RegisterSpeechRecognizer},
    {"TimeZoneMonitorAndroid", content::TimeZoneMonitorAndroid::Register},
    {"TracingControllerAndroid", content::RegisterTracingControllerAndroid},
    {"WebContentsAndroid", content::WebContentsAndroid::Register},
    {"WebContentsObserver", content::RegisterWebContentsObserverProxy},
    {"WebViewStatics", content::RegisterWebViewStatics},
};

}  // namespace

namespace content {
namespace android {

bool RegisterBrowserJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
