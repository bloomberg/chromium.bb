// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents.h"

#include <limits>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/gpu_memory_buffer_factory_impl.h"
#include "android_webview/browser/hardware_renderer.h"
#include "android_webview/browser/net_disk_cache_remover.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/native/aw_autofill_manager_delegate.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "android_webview/native/aw_contents_client_bridge.h"
#include "android_webview/native/aw_contents_io_thread_client_impl.h"
#include "android_webview/native/aw_pdf_exporter.h"
#include "android_webview/native/aw_picture.h"
#include "android_webview/native/aw_web_contents_delegate.h"
#include "android_webview/native/java_browser_view_renderer_helper.h"
#include "android_webview/native/state_serializer.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "jni/AwContents_jni.h"
#include "net/cert/x509_certificate.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"

struct AwDrawSWFunctionTable;
struct AwDrawGLFunctionTable;

using autofill::ContentAutofillDriver;
using autofill::AutofillManager;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using navigation_interception::InterceptNavigationDelegate;
using content::BrowserThread;
using content::ContentViewCore;
using content::WebContents;

extern "C" {
static AwDrawGLFunction DrawGLFunction;
static void DrawGLFunction(long view_context,
                           AwDrawGLInfo* draw_info,
                           void* spare) {
  // |view_context| is the value that was returned from the java
  // AwContents.onPrepareDrawGL; this cast must match the code there.
  reinterpret_cast<android_webview::AwContents*>(view_context)
      ->DrawGL(draw_info);
}
}

namespace android_webview {

namespace {

bool g_should_download_favicons = false;

const void* kAwContentsUserDataKey = &kAwContentsUserDataKey;

class AwContentsUserData : public base::SupportsUserData::Data {
 public:
  AwContentsUserData(AwContents* ptr) : contents_(ptr) {}

  static AwContents* GetContents(WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    AwContentsUserData* data = reinterpret_cast<AwContentsUserData*>(
        web_contents->GetUserData(kAwContentsUserDataKey));
    return data ? data->contents_ : NULL;
  }

 private:
  AwContents* contents_;
};

base::subtle::Atomic32 g_instance_count = 0;

// TODO(boliu): Deduplicate with chrome/ code.
content::RendererPreferencesHintingEnum GetRendererPreferencesHintingEnum(
    gfx::FontRenderParams::Hinting hinting) {
  switch (hinting) {
    case gfx::FontRenderParams::HINTING_NONE:
      return content::RENDERER_PREFERENCES_HINTING_NONE;
    case gfx::FontRenderParams::HINTING_SLIGHT:
      return content::RENDERER_PREFERENCES_HINTING_SLIGHT;
    case gfx::FontRenderParams::HINTING_MEDIUM:
      return content::RENDERER_PREFERENCES_HINTING_MEDIUM;
    case gfx::FontRenderParams::HINTING_FULL:
      return content::RENDERER_PREFERENCES_HINTING_FULL;
    default:
      NOTREACHED() << "Unhandled hinting style " << hinting;
      return content::RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT;
  }
}

// TODO(boliu): Deduplicate with chrome/ code.
content::RendererPreferencesSubpixelRenderingEnum
GetRendererPreferencesSubpixelRenderingEnum(
    gfx::FontRenderParams::SubpixelRendering subpixel_rendering) {
  switch (subpixel_rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR;
    default:
      NOTREACHED() << "Unhandled subpixel rendering style "
                   << subpixel_rendering;
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT;
  }
}

void OnIoThreadClientReady(content::RenderFrameHost* rfh) {
  int render_process_id = rfh->GetProcess()->GetID();
  int render_frame_id = rfh->GetRoutingID();
  AwResourceDispatcherHostDelegate::OnIoThreadClientReady(
      render_process_id, render_frame_id);
}

}  // namespace

// static
AwContents* AwContents::FromWebContents(WebContents* web_contents) {
  return AwContentsUserData::GetContents(web_contents);
}

// static
AwContents* AwContents::FromID(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh) return NULL;
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents) return NULL;
  return FromWebContents(web_contents);
}

AwContents::AwContents(scoped_ptr<WebContents> web_contents)
    : weak_factory_on_ui_thread_(this),
      ui_thread_weak_ptr_(weak_factory_on_ui_thread_.GetWeakPtr()),
      web_contents_(web_contents.Pass()),
      shared_renderer_state_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          this),
      browser_view_renderer_(
          this,
          &shared_renderer_state_,
          web_contents_.get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)) {
  base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, 1);
  icon_helper_.reset(new IconHelper(web_contents_.get()));
  icon_helper_->SetListener(this);
  web_contents_->SetUserData(kAwContentsUserDataKey,
                             new AwContentsUserData(this));
  render_view_host_ext_.reset(
      new AwRenderViewHostExt(this, web_contents_.get()));

  AwAutofillManagerDelegate* autofill_manager_delegate =
      AwAutofillManagerDelegate::FromWebContents(web_contents_.get());
  if (autofill_manager_delegate)
    InitAutofillIfNecessary(autofill_manager_delegate->GetSaveFormData());

  SetAndroidWebViewRendererPrefs();
}

void AwContents::SetJavaPeers(JNIEnv* env,
                              jobject obj,
                              jobject aw_contents,
                              jobject web_contents_delegate,
                              jobject contents_client_bridge,
                              jobject io_thread_client,
                              jobject intercept_navigation_delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The |aw_content| param is technically spurious as it duplicates |obj| but
  // is passed over anyway to make the binding more explicit.
  java_ref_ = JavaObjectWeakGlobalRef(env, aw_contents);

  web_contents_delegate_.reset(
      new AwWebContentsDelegate(env, web_contents_delegate));
  web_contents_->SetDelegate(web_contents_delegate_.get());

  contents_client_bridge_.reset(
      new AwContentsClientBridge(env, contents_client_bridge));
  AwContentsClientBridgeBase::Associate(web_contents_.get(),
                                        contents_client_bridge_.get());

  AwContentsIoThreadClientImpl::Associate(
      web_contents_.get(), ScopedJavaLocalRef<jobject>(env, io_thread_client));

  InterceptNavigationDelegate::Associate(
      web_contents_.get(),
      make_scoped_ptr(new InterceptNavigationDelegate(
          env, intercept_navigation_delegate)));

  // Finally, having setup the associations, release any deferred requests
  web_contents_->ForEachFrame(base::Bind(&OnIoThreadClientReady));
}

void AwContents::SetSaveFormData(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InitAutofillIfNecessary(enabled);
  // We need to check for the existence, since autofill_manager_delegate
  // may not be created when the setting is false.
  if (ContentAutofillDriver::FromWebContents(web_contents_.get())) {
    AwAutofillManagerDelegate::FromWebContents(web_contents_.get())->
        SetSaveFormData(enabled);
  }
}

void AwContents::InitAutofillIfNecessary(bool enabled) {
  // Do not initialize if the feature is not enabled.
  if (!enabled)
    return;
  // Check if the autofill driver already exists.
  content::WebContents* web_contents = web_contents_.get();
  if (ContentAutofillDriver::FromWebContents(web_contents))
    return;

  AwBrowserContext::FromWebContents(web_contents)->
      CreateUserPrefServiceIfNecessary();
  AwAutofillManagerDelegate::CreateForWebContents(web_contents);
  ContentAutofillDriver::CreateForWebContentsAndDelegate(
      web_contents,
      AwAutofillManagerDelegate::FromWebContents(web_contents),
      l10n_util::GetDefaultLocale(),
      AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER);
}

void AwContents::SetAndroidWebViewRendererPrefs() {
  content::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  prefs->tap_multiple_targets_strategy =
      content::TAP_MULTIPLE_TARGETS_STRATEGY_NONE;

  // TODO(boliu): Deduplicate with chrome/ code.
  const gfx::FontRenderParams& params = gfx::GetDefaultWebKitFontRenderParams();
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = GetRendererPreferencesHintingEnum(params.hinting);
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering =
      GetRendererPreferencesSubpixelRenderingEnum(params.subpixel_rendering);

  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (host)
    host->SyncRendererPrefs();
}

void AwContents::SetAwAutofillManagerDelegate(jobject delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_setAwAutofillManagerDelegate(env, obj.obj(), delegate);
}

AwContents::~AwContents() {
  DCHECK(AwContents::FromWebContents(web_contents_.get()) == this);
  DCHECK(!hardware_renderer_.get());
  web_contents_->RemoveUserData(kAwContentsUserDataKey);
  if (find_helper_.get())
    find_helper_->SetListener(NULL);
  if (icon_helper_.get())
    icon_helper_->SetListener(NULL);
  base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, -1);
  // When the last WebView is destroyed free all discardable memory allocated by
  // Chromium, because the app process may continue to run for a long time
  // without ever using another WebView.
  if (base::subtle::NoBarrier_Load(&g_instance_count) == 0) {
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_CRITICAL);
  }
}

jlong AwContents::GetWebContents(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(web_contents_);
  return reinterpret_cast<intptr_t>(web_contents_.get());
}

void AwContents::Destroy(JNIEnv* env, jobject obj) {
  java_ref_.reset();
  // We do not delete AwContents immediately. Some applications try to delete
  // Webview in ShouldOverrideUrlLoading callback, which is a sync IPC from
  // Webkit.
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

static jlong Init(JNIEnv* env, jclass, jobject browser_context) {
  // TODO(joth): Use |browser_context| to get the native BrowserContext, rather
  // than hard-code the default instance lookup here.
  scoped_ptr<WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(AwBrowserContext::GetDefault())));
  // Return an 'uninitialized' instance; most work is deferred until the
  // subsequent SetJavaPeers() call.
  return reinterpret_cast<intptr_t>(new AwContents(web_contents.Pass()));
}

static void SetAwDrawSWFunctionTable(JNIEnv* env, jclass,
                                     jlong function_table) {
  JavaBrowserViewRendererHelper::SetAwDrawSWFunctionTable(
      reinterpret_cast<AwDrawSWFunctionTable*>(function_table));
}

static void SetAwDrawGLFunctionTable(JNIEnv* env, jclass,
                                     jlong function_table) {
  GpuMemoryBufferFactoryImpl::SetAwDrawGLFunctionTable(
      reinterpret_cast<AwDrawGLFunctionTable*>(function_table));
}

static jlong GetAwDrawGLFunction(JNIEnv* env, jclass) {
  return reinterpret_cast<intptr_t>(&DrawGLFunction);
}

// static
jint GetNativeInstanceCount(JNIEnv* env, jclass) {
  return base::subtle::NoBarrier_Load(&g_instance_count);
}

jlong AwContents::GetAwDrawGLViewContext(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return reinterpret_cast<intptr_t>(this);
}

void AwContents::DrawGL(AwDrawGLInfo* draw_info) {
  if (!hardware_renderer_) {
    // TODO(boliu): Use executeHardwareAction to synchronously initialize
    // hardware on first functor request. Then functor can point directly
    // to HardwareRenderer.
    hardware_renderer_.reset(new HardwareRenderer(&shared_renderer_state_));
  }

  DrawGLResult result;
  if (hardware_renderer_->DrawGL(draw_info, &result)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AwContents::DidDrawGL, ui_thread_weak_ptr_, result));
  }
}

void AwContents::DidDrawGL(const DrawGLResult& result) {
  browser_view_renderer_.DidDrawGL(result);
}

namespace {
void DocumentHasImagesCallback(const ScopedJavaGlobalRef<jobject>& message,
                               bool has_images) {
  Java_AwContents_onDocumentHasImagesResponse(AttachCurrentThread(),
                                              has_images,
                                              message.obj());
}
}  // namespace

void AwContents::DocumentHasImages(JNIEnv* env, jobject obj, jobject message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ScopedJavaGlobalRef<jobject> j_message;
  j_message.Reset(env, message);
  render_view_host_ext_->DocumentHasImages(
      base::Bind(&DocumentHasImagesCallback, j_message));
}

namespace {
void GenerateMHTMLCallback(ScopedJavaGlobalRef<jobject>* callback,
                           const base::FilePath& path, int64 size) {
  JNIEnv* env = AttachCurrentThread();
  // Android files are UTF8, so the path conversion below is safe.
  Java_AwContents_generateMHTMLCallback(
      env,
      ConvertUTF8ToJavaString(env, path.AsUTF8Unsafe()).obj(),
      size, callback->obj());
}
}  // namespace

void AwContents::GenerateMHTML(JNIEnv* env, jobject obj,
                               jstring jpath, jobject callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ScopedJavaGlobalRef<jobject>* j_callback = new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, callback);
  base::FilePath target_path(ConvertJavaStringToUTF8(env, jpath));
  web_contents_->GenerateMHTML(
      target_path,
      base::Bind(&GenerateMHTMLCallback, base::Owned(j_callback), target_path));
}

void AwContents::CreatePdfExporter(JNIEnv* env,
                                   jobject obj,
                                   jobject pdfExporter) {
  pdf_exporter_.reset(
      new AwPdfExporter(env,
                        pdfExporter,
                        web_contents_.get()));
}

bool AwContents::OnReceivedHttpAuthRequest(const JavaRef<jobject>& handler,
                                           const std::string& host,
                                           const std::string& realm) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  ScopedJavaLocalRef<jstring> jhost = ConvertUTF8ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "onReceivedHttpAuthRequest");
  Java_AwContents_onReceivedHttpAuthRequest(env, obj.obj(), handler.obj(),
      jhost.obj(), jrealm.obj());
  return true;
}

void AwContents::AddVisitedLinks(JNIEnv* env,
                                   jobject obj,
                                   jobjectArray jvisited_links) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<base::string16> visited_link_strings;
  base::android::AppendJavaStringArrayToStringVector(
      env, jvisited_links, &visited_link_strings);

  std::vector<GURL> visited_link_gurls;
  std::vector<base::string16>::const_iterator itr;
  for (itr = visited_link_strings.begin(); itr != visited_link_strings.end();
       ++itr) {
    visited_link_gurls.push_back(GURL(*itr));
  }

  AwBrowserContext::FromWebContents(web_contents_.get())
      ->AddVisitedURLs(visited_link_gurls);
}

bool RegisterAwContents(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

namespace {

void ShowGeolocationPromptHelperTask(const JavaObjectWeakGlobalRef& java_ref,
                                     const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref.get(env);
  if (j_ref.obj()) {
    ScopedJavaLocalRef<jstring> j_origin(
        ConvertUTF8ToJavaString(env, origin.spec()));
    devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
        "onGeolocationPermissionsShowPrompt");
    Java_AwContents_onGeolocationPermissionsShowPrompt(env,
                                                       j_ref.obj(),
                                                       j_origin.obj());
  }
}

void ShowGeolocationPromptHelper(const JavaObjectWeakGlobalRef& java_ref,
                                 const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  if (java_ref.get(env).obj()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ShowGeolocationPromptHelperTask,
                   java_ref,
                   origin));
  }
}

} // anonymous namespace

void AwContents::ShowGeolocationPrompt(const GURL& requesting_frame,
                                       base::Callback<void(bool)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GURL origin = requesting_frame.GetOrigin();
  bool show_prompt = pending_geolocation_prompts_.empty();
  pending_geolocation_prompts_.push_back(OriginCallback(origin, callback));
  if (show_prompt) {
    ShowGeolocationPromptHelper(java_ref_, origin);
  }
}

// Invoked from Java
void AwContents::InvokeGeolocationCallback(JNIEnv* env,
                                           jobject obj,
                                           jboolean value,
                                           jstring origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GURL callback_origin(base::android::ConvertJavaStringToUTF16(env, origin));
  if (callback_origin.GetOrigin() ==
      pending_geolocation_prompts_.front().first) {
    pending_geolocation_prompts_.front().second.Run(value);
    pending_geolocation_prompts_.pop_front();
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_,
                                  pending_geolocation_prompts_.front().first);
    }
  }
}

void AwContents::HideGeolocationPrompt(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool removed_current_outstanding_callback = false;
  std::list<OriginCallback>::iterator it = pending_geolocation_prompts_.begin();
  while (it != pending_geolocation_prompts_.end()) {
    if ((*it).first == origin.GetOrigin()) {
      if (it == pending_geolocation_prompts_.begin()) {
        removed_current_outstanding_callback = true;
      }
      it = pending_geolocation_prompts_.erase(it);
    } else {
      ++it;
    }
  }

  if (removed_current_outstanding_callback) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
    if (j_ref.obj()) {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onGeolocationPermissionsHidePrompt");
      Java_AwContents_onGeolocationPermissionsHidePrompt(env, j_ref.obj());
    }
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_,
                            pending_geolocation_prompts_.front().first);
    }
  }
}

void AwContents::FindAllAsync(JNIEnv* env, jobject obj, jstring search_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void AwContents::FindNext(JNIEnv* env, jobject obj, jboolean forward) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetFindHelper()->FindNext(forward);
}

void AwContents::ClearMatches(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetFindHelper()->ClearMatches();
}

void AwContents::ClearCache(
    JNIEnv* env,
    jobject obj,
    jboolean include_disk_files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->ClearCache();

  if (include_disk_files) {
    RemoveHttpDiskCache(web_contents_->GetBrowserContext(),
                        web_contents_->GetRoutingID());
  }
}

FindHelper* AwContents::GetFindHelper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!find_helper_.get()) {
    find_helper_.reset(new FindHelper(web_contents_.get()));
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

void AwContents::OnFindResultReceived(int active_ordinal,
                                      int match_count,
                                      bool finished) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onFindResultReceived(
      env, obj.obj(), active_ordinal, match_count, finished);
}

bool AwContents::ShouldDownloadFavicon(const GURL& icon_url) {
  return g_should_download_favicons;
}

void AwContents::OnReceivedIcon(const GURL& icon_url, const SkBitmap& bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();

  if (entry) {
    entry->GetFavicon().valid = true;
    entry->GetFavicon().url = icon_url;
    entry->GetFavicon().image = gfx::Image::CreateFrom1xBitmap(bitmap);
  }

  Java_AwContents_onReceivedIcon(
      env, obj.obj(), gfx::ConvertToJavaBitmap(&bitmap).obj());
}

void AwContents::OnReceivedTouchIconUrl(const std::string& url,
                                        bool precomposed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onReceivedTouchIconUrl(
      env, obj.obj(), ConvertUTF8ToJavaString(env, url).obj(), precomposed);
}

bool AwContents::RequestDrawGL(jobject canvas) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_AwContents_requestDrawGL(env, obj.obj(), canvas);
}

void AwContents::PostInvalidate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_AwContents_postInvalidateOnAnimation(env, obj.obj());
}

void AwContents::OnNewPicture() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
        "onNewPicture");
    Java_AwContents_onNewPicture(env, obj.obj());
  }
}

base::android::ScopedJavaLocalRef<jbyteArray>
    AwContents::GetCertificate(JNIEnv* env,
                               jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  if (!entry)
    return ScopedJavaLocalRef<jbyteArray>();
  // Get the certificate
  int cert_id = entry->GetSSL().cert_id;
  scoped_refptr<net::X509Certificate> cert;
  bool ok = content::CertStore::GetInstance()->RetrieveCert(cert_id, &cert);
  if (!ok)
    return ScopedJavaLocalRef<jbyteArray>();

  // Convert the certificate and return it
  std::string der_string;
  net::X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_string);
  return base::android::ToJavaByteArray(env,
      reinterpret_cast<const uint8*>(der_string.data()), der_string.length());
}

void AwContents::RequestNewHitTestDataAt(JNIEnv* env, jobject obj,
                                         jint x, jint y) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->RequestNewHitTestDataAt(x, y);
}

void AwContents::UpdateLastHitTestData(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!render_view_host_ext_->HasNewHitTestData()) return;

  const AwHitTestData& data = render_view_host_ext_->GetLastHitTestData();
  render_view_host_ext_->MarkHitTestDataRead();

  // Make sure to null the Java object if data is empty/invalid.
  ScopedJavaLocalRef<jstring> extra_data_for_type;
  if (data.extra_data_for_type.length())
    extra_data_for_type = ConvertUTF8ToJavaString(
        env, data.extra_data_for_type);

  ScopedJavaLocalRef<jstring> href;
  if (data.href.length())
    href = ConvertUTF16ToJavaString(env, data.href);

  ScopedJavaLocalRef<jstring> anchor_text;
  if (data.anchor_text.length())
    anchor_text = ConvertUTF16ToJavaString(env, data.anchor_text);

  ScopedJavaLocalRef<jstring> img_src;
  if (data.img_src.is_valid())
    img_src = ConvertUTF8ToJavaString(env, data.img_src.spec());

  Java_AwContents_updateHitTestData(env,
                                    obj,
                                    data.type,
                                    extra_data_for_type.obj(),
                                    href.obj(),
                                    anchor_text.obj(),
                                    img_src.obj());
}

void AwContents::OnSizeChanged(JNIEnv* env, jobject obj,
                               int w, int h, int ow, int oh) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.OnSizeChanged(w, h);
}

void AwContents::SetViewVisibility(JNIEnv* env, jobject obj, bool visible) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.SetViewVisibility(visible);
}

void AwContents::SetWindowVisibility(JNIEnv* env, jobject obj, bool visible) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.SetWindowVisibility(visible);
}

void AwContents::SetIsPaused(JNIEnv* env, jobject obj, bool paused) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.SetIsPaused(paused);
  ContentViewCore* cvc =
      ContentViewCore::FromWebContents(web_contents_.get());
  if (cvc) {
    cvc->PauseOrResumeGeolocation(paused);
    if (paused) {
      cvc->PauseVideo();
    }
  }
}

void AwContents::OnAttachedToWindow(JNIEnv* env, jobject obj, int w, int h) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.OnAttachedToWindow(w, h);
}

void AwContents::OnDetachedFromWindow(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.OnDetachedFromWindow();
}

void AwContents::ReleaseHardwareDrawOnRenderThread(JNIEnv* env, jobject obj) {
  hardware_renderer_.reset();
}

base::android::ScopedJavaLocalRef<jbyteArray>
AwContents::GetOpaqueState(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Required optimization in WebViewClassic to not save any state if
  // there has been no navigations.
  if (!web_contents_->GetController().GetEntryCount())
    return ScopedJavaLocalRef<jbyteArray>();

  Pickle pickle;
  if (!WriteToPickle(*web_contents_, &pickle)) {
    return ScopedJavaLocalRef<jbyteArray>();
  } else {
    return base::android::ToJavaByteArray(env,
       reinterpret_cast<const uint8*>(pickle.data()), pickle.size());
  }
}

jboolean AwContents::RestoreFromOpaqueState(
    JNIEnv* env, jobject obj, jbyteArray state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(boliu): This copy can be optimized out if this is a performance
  // problem.
  std::vector<uint8> state_vector;
  base::android::JavaByteArrayToByteVector(env, state, &state_vector);

  Pickle pickle(reinterpret_cast<const char*>(state_vector.begin()),
                state_vector.size());
  PickleIterator iterator(pickle);

  return RestoreFromPickle(&iterator, web_contents_.get());
}

bool AwContents::OnDraw(JNIEnv* env,
                        jobject obj,
                        jobject canvas,
                        jboolean is_hardware_accelerated,
                        jint scroll_x,
                        jint scroll_y,
                        jint visible_left,
                        jint visible_top,
                        jint visible_right,
                        jint visible_bottom,
                        jint clip_left,
                        jint clip_top,
                        jint clip_right,
                        jint clip_bottom) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return browser_view_renderer_.OnDraw(
      canvas,
      is_hardware_accelerated,
      gfx::Vector2d(scroll_x, scroll_y),
      gfx::Rect(visible_left,
                visible_top,
                visible_right - visible_left,
                visible_bottom - visible_top),
      gfx::Rect(
          clip_left, clip_top, clip_right - clip_left, clip_bottom - clip_top));
}

void AwContents::SetPendingWebContentsForPopup(
    scoped_ptr<content::WebContents> pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (pending_contents_.get()) {
    // TODO(benm): Support holding multiple pop up window requests.
    LOG(WARNING) << "Blocking popup window creation as an outstanding "
                 << "popup window is still pending.";
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, pending.release());
    return;
  }
  pending_contents_.reset(new AwContents(pending.Pass()));
}

void AwContents::FocusFirstNode(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_contents_->FocusThroughTabTraversal(false);
}

void AwContents::SetBackgroundColor(JNIEnv* env, jobject obj, jint color) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetBackgroundColor(color);
}

jlong AwContents::ReleasePopupAwContents(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return reinterpret_cast<intptr_t>(pending_contents_.release());
}

gfx::Point AwContents::GetLocationOnScreen() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return gfx::Point();
  std::vector<int> location;
  base::android::JavaIntArrayToIntVector(
      env,
      Java_AwContents_getLocationOnScreen(env, obj.obj()).obj(),
      &location);
  return gfx::Point(location[0], location[1]);
}

void AwContents::SetMaxContainerViewScrollOffset(gfx::Vector2d new_value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_setMaxContainerViewScrollOffset(
      env, obj.obj(), new_value.x(), new_value.y());
}

void AwContents::ScrollContainerViewTo(gfx::Vector2d new_value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_scrollContainerViewTo(
      env, obj.obj(), new_value.x(), new_value.y());
}

bool AwContents::IsFlingActive() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_AwContents_isFlingActive(env, obj.obj());
}

void AwContents::SetPageScaleFactorAndLimits(
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_setPageScaleFactorAndLimits(env,
                                              obj.obj(),
                                              page_scale_factor,
                                              min_page_scale_factor,
                                              max_page_scale_factor);
}

void AwContents::SetContentsSize(gfx::SizeF contents_size_dip) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_setContentsSize(
      env, obj.obj(), contents_size_dip.width(), contents_size_dip.height());
}

void AwContents::DidOverscroll(gfx::Vector2d overscroll_delta) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_didOverscroll(
      env, obj.obj(), overscroll_delta.x(), overscroll_delta.y());
}

const BrowserViewRenderer* AwContents::GetBrowserViewRenderer() const {
  return &browser_view_renderer_;
}

void AwContents::SetDipScale(JNIEnv* env, jobject obj, jfloat dip_scale) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.SetDipScale(dip_scale);
}

void AwContents::SetFixedLayoutSize(JNIEnv* env,
                                    jobject obj,
                                    jint width_dip,
                                    jint height_dip) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetFixedLayoutSize(gfx::Size(width_dip, height_dip));
}

void AwContents::ScrollTo(JNIEnv* env, jobject obj, jint x, jint y) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.ScrollTo(gfx::Vector2d(x, y));
}

void AwContents::OnWebLayoutPageScaleFactorChanged(float page_scale_factor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_onWebLayoutPageScaleFactorChanged(env, obj.obj(),
                                                         page_scale_factor);
}

void AwContents::OnWebLayoutContentsSizeChanged(
    const gfx::Size& contents_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_onWebLayoutContentsSizeChanged(
      env, obj.obj(), contents_size.width(), contents_size.height());
}

jlong AwContents::CapturePicture(JNIEnv* env,
                                 jobject obj,
                                 int width,
                                 int height) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return reinterpret_cast<intptr_t>(
      new AwPicture(browser_view_renderer_.CapturePicture(width, height)));
}

void AwContents::EnableOnNewPicture(JNIEnv* env,
                                    jobject obj,
                                    jboolean enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.EnableOnNewPicture(enabled);
}

void AwContents::ClearView(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_view_renderer_.ClearView();
}

void AwContents::SetExtraHeadersForUrl(JNIEnv* env, jobject obj,
                                       jstring url, jstring jextra_headers) {
  std::string extra_headers;
  if (jextra_headers)
    extra_headers = ConvertJavaStringToUTF8(env, jextra_headers);
  AwResourceContext* resource_context = static_cast<AwResourceContext*>(
      AwBrowserContext::FromWebContents(web_contents_.get())->
      GetResourceContext());
  resource_context->SetExtraHeaders(GURL(ConvertJavaStringToUTF8(env, url)),
                                    extra_headers);
}

void AwContents::SetJsOnlineProperty(JNIEnv* env,
                                     jobject obj,
                                     jboolean network_up) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetJsOnlineProperty(network_up);
}

void AwContents::TrimMemoryOnRenderThread(JNIEnv* env,
                                          jobject obj,
                                          jint level,
                                          jboolean visible) {
  if (hardware_renderer_) {
    if (hardware_renderer_->TrimMemory(level, visible)) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(&AwContents::ForceFakeComposite, ui_thread_weak_ptr_));
    }
  }
}

void AwContents::ForceFakeComposite() {
  browser_view_renderer_.ForceFakeCompositeSW();
}

void SetShouldDownloadFavicons(JNIEnv* env, jclass jclazz) {
  g_should_download_favicons = true;
}

}  // namespace android_webview
