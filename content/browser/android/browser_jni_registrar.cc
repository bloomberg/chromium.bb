// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/app_web_message_port.h"
#include "content/browser/android/background_sync_network_observer_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/content_feature_list.h"
#include "content/browser/android/content_video_view.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_render_view.h"
#include "content/browser/android/content_view_statics.h"
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/dialog_overlay_impl.h"
#include "content/browser/android/gpu_process_callback.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/selection_popup_controller.h"
#include "content/browser/android/smart_selection_client.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/android/web_contents_observer_proxy.h"
#include "content/browser/child_process_launcher_helper_android.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/frame_host/render_frame_host_android.h"
#include "content/browser/media/session/audio_focus_delegate_android.h"
#include "content/browser/media/session/media_session_android.h"
#include "content/browser/memory/memory_monitor_android.h"
#include "content/browser/speech/speech_recognizer_impl_android.h"
#include "content/browser/web_contents/web_contents_android.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
    {"AppWebMessagePort", content::RegisterAppWebMessagePort},
    {"AudioFocusDelegate", content::AudioFocusDelegateAndroid::Register},
    {"BrowserStartupController", content::RegisterBrowserStartupController},
    {"ChildProcessLauncher", content::RegisterChildProcessLauncher},
    {"ContentFeatureList", content::android::RegisterContentFeatureListJni},
    {"ContentVideoView", content::ContentVideoView::RegisterContentVideoView},
    {"GpuProcessCallback", content::RegisterGpuProcessCallback},
    {"MemoryMonitorAndroid", content::MemoryMonitorAndroid::Register},
    {"BackgroundSyncNetworkObserverAndroid",
     content::BackgroundSyncNetworkObserverAndroid::Observer::
         RegisterNetworkObserver},
    {"BrowserAccessibilityManager",
     content::RegisterBrowserAccessibilityManager},
    {"ContentViewCore", content::RegisterContentViewCore},
    {"ContentViewRenderView",
     content::ContentViewRenderView::RegisterContentViewRenderView},
    {"DateTimePickerAndroid", content::RegisterDateTimeChooserAndroid},
    {"DialogOverlayImpl",
     content::DialogOverlayImpl::RegisterDialogOverlayImpl},
    {"InterstitialPageDelegateAndroid",
     content::InterstitialPageDelegateAndroid::
         RegisterInterstitialPageDelegateAndroid},
    {"LoadUrlParams", content::RegisterLoadUrlParams},
    {"MediaSessionImpl", content::MediaSessionAndroid::Register},
    {"NavigationControllerAndroid",
     content::NavigationControllerAndroid::Register},
    {"RegisterImeAdapter", content::RegisterImeAdapter},
    {"RegisterSelectionPopupController",
     content::RegisterSelectionPopupController},
    {"RenderFrameHostAndroid", content::RenderFrameHostAndroid::Register},
    {"SmartSelectionClient", content::RegisterSmartSelectionClient},
    {"SpeechRecognizerImplAndroid",
     content::SpeechRecognizerImplAndroid::RegisterSpeechRecognizer},
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
