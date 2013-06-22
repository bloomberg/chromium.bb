// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/gpu_memory_buffer_impl.h"
#include "android_webview/browser/in_process_view_renderer.h"
#include "android_webview/browser/net_disk_cache_remover.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "android_webview/native/aw_contents_client_bridge.h"
#include "android_webview/native/aw_contents_io_thread_client_impl.h"
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
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "jni/AwContents_jni.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/android/java_bitmap.h"

struct AwDrawSWFunctionTable;
struct AwDrawGLFunctionTable;

using autofill::AutofillDriverImpl;
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
static void DrawGLFunction(int view_context,
                           AwDrawGLInfo* draw_info,
                           void* spare) {
  // |view_context| is the value that was returned from the java
  // AwContents.onPrepareDrawGL; this cast must match the code there.
  reinterpret_cast<android_webview::BrowserViewRenderer*>(view_context)->DrawGL(
      draw_info);
}
}

namespace android_webview {

namespace {

JavaBrowserViewRendererHelper* java_renderer_helper() {
  static JavaBrowserViewRendererHelper* g_instance
      = new JavaBrowserViewRendererHelper;
  return g_instance;
}

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
    : web_contents_(web_contents.Pass()),
      browser_view_renderer_(
          new InProcessViewRenderer(this, java_renderer_helper(),
                                    web_contents_.get())) {
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
  int child_id = web_contents_->GetRenderProcessHost()->GetID();
  int route_id = web_contents_->GetRoutingID();
  AwResourceDispatcherHostDelegate::OnIoThreadClientReady(child_id, route_id);

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InterceptNavigationDelegate::Associate(
      web_contents_.get(),
      make_scoped_ptr(new InterceptNavigationDelegate(
          env, intercept_navigation_delegate)));
}

void AwContents::SetSaveFormData(bool enabled) {
  InitAutofillIfNecessary(enabled);
  // We need to check for the existence, since autofill_manager_delegate
  // may not be created when the setting is false.
  if (AutofillDriverImpl::FromWebContents(web_contents_.get())) {
    AwAutofillManagerDelegate::FromWebContents(web_contents_.get())->
        SetSaveFormData(enabled);
  }
}

void AwContents::InitAutofillIfNecessary(bool enabled) {
  // Do not initialize if the feature is not enabled.
  if (!enabled)
    return;
  // Check if the autofill manager already exists.
  content::WebContents* web_contents = web_contents_.get();
  if (AutofillDriverImpl::FromWebContents(web_contents))
    return;

  AwBrowserContext::FromWebContents(web_contents)->
      CreateUserPrefServiceIfNecessary();
  AwAutofillManagerDelegate::CreateForWebContents(web_contents);
  AutofillDriverImpl::CreateForWebContentsAndDelegate(
      web_contents,
      AwAutofillManagerDelegate::FromWebContents(web_contents),
      l10n_util::GetDefaultLocale(),
      AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER);
}

AwContents::~AwContents() {
  DCHECK(AwContents::FromWebContents(web_contents_.get()) == this);
  web_contents_->RemoveUserData(kAwContentsUserDataKey);
  if (find_helper_.get())
    find_helper_->SetListener(NULL);
  if (icon_helper_.get())
    icon_helper_->SetListener(NULL);
  base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, -1);
}

jint AwContents::GetWebContents(JNIEnv* env, jobject obj) {
  DCHECK(web_contents_);
  return reinterpret_cast<jint>(web_contents_.get());
}

void AwContents::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

static jint Init(JNIEnv* env, jclass, jobject browser_context) {
  // TODO(joth): Use |browser_context| to get the native BrowserContext, rather
  // than hard-code the default instance lookup here.
  scoped_ptr<WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(AwBrowserContext::GetDefault())));
  // Return an 'uninitialized' instance; most work is deferred until the
  // subsequent SetJavaPeers() call.
  return reinterpret_cast<jint>(new AwContents(web_contents.Pass()));
}

static void SetAwDrawSWFunctionTable(JNIEnv* env, jclass, jint function_table) {
  BrowserViewRenderer::SetAwDrawSWFunctionTable(
      reinterpret_cast<AwDrawSWFunctionTable*>(function_table));
}

static void SetAwDrawGLFunctionTable(JNIEnv* env, jclass, jint function_table) {
  GpuMemoryBufferImpl::SetAwDrawGLFunctionTable(
      reinterpret_cast<AwDrawGLFunctionTable*>(function_table));
}

static jint GetAwDrawGLFunction(JNIEnv* env, jclass) {
  return reinterpret_cast<jint>(&DrawGLFunction);
}

// static
jint GetNativeInstanceCount(JNIEnv* env, jclass) {
  return base::subtle::NoBarrier_Load(&g_instance_count);
}

jint AwContents::GetAwDrawGLViewContext(JNIEnv* env, jobject obj) {
  return reinterpret_cast<jint>(browser_view_renderer_.get());
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
  ScopedJavaGlobalRef<jobject>* j_callback = new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, callback);
  web_contents_->GenerateMHTML(
      base::FilePath(ConvertJavaStringToUTF8(env, jpath)),
      base::Bind(&GenerateMHTMLCallback, base::Owned(j_callback)));
}

void AwContents::PerformLongClick() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_performLongClick(env, obj.obj());
}

bool AwContents::OnReceivedHttpAuthRequest(const JavaRef<jobject>& handler,
                                           const std::string& host,
                                           const std::string& realm) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  ScopedJavaLocalRef<jstring> jhost = ConvertUTF8ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  Java_AwContents_onReceivedHttpAuthRequest(env, obj.obj(), handler.obj(),
      jhost.obj(), jrealm.obj());
  return true;
}

void AwContents::AddVisitedLinks(JNIEnv* env,
                                   jobject obj,
                                   jobjectArray jvisited_links) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<string16> visited_link_strings;
  base::android::AppendJavaStringArrayToStringVector(
      env, jvisited_links, &visited_link_strings);

  std::vector<GURL> visited_link_gurls;
  for (std::vector<string16>::const_iterator itr = visited_link_strings.begin();
       itr != visited_link_strings.end();
       ++itr) {
    visited_link_gurls.push_back(GURL(*itr));
  }

  AwBrowserContext::FromWebContents(web_contents_.get())
      ->AddVisitedURLs(visited_link_gurls);
}

bool RegisterAwContents(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

namespace {

void ShowGeolocationPromptHelperTask(const JavaObjectWeakGlobalRef& java_ref,
                                     const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref.get(env);
  if (j_ref.obj()) {
    ScopedJavaLocalRef<jstring> j_origin(
        ConvertUTF8ToJavaString(env, origin.spec()));
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
      Java_AwContents_onGeolocationPermissionsHidePrompt(env, j_ref.obj());
    }
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_,
                            pending_geolocation_prompts_.front().first);
    }
  }
}

void AwContents::FindAllAsync(JNIEnv* env, jobject obj, jstring search_string) {
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void AwContents::FindNext(JNIEnv* env, jobject obj, jboolean forward) {
  GetFindHelper()->FindNext(forward);
}

void AwContents::ClearMatches(JNIEnv* env, jobject obj) {
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
  if (!find_helper_.get()) {
    find_helper_.reset(new FindHelper(web_contents_.get()));
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

void AwContents::OnFindResultReceived(int active_ordinal,
                                      int match_count,
                                      bool finished) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onFindResultReceived(
      env, obj.obj(), active_ordinal, match_count, finished);
}

void AwContents::OnReceivedIcon(const SkBitmap& bitmap) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onReceivedIcon(
      env, obj.obj(), gfx::ConvertToJavaBitmap(&bitmap).obj());

  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();

  if (!entry || entry->GetURL().is_empty())
    return;

  // TODO(acleung): Get the last history entry and set
  // the icon.
}

void AwContents::OnReceivedTouchIconUrl(const std::string& url,
                                        bool precomposed) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onReceivedTouchIconUrl(
      env, obj.obj(), ConvertUTF8ToJavaString(env, url).obj(), precomposed);
}

bool AwContents::RequestDrawGL(jobject canvas) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_AwContents_requestDrawGL(env, obj.obj(), canvas);
}

void AwContents::PostInvalidate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_AwContents_postInvalidateOnAnimation(env, obj.obj());
}

void AwContents::OnNewPicture() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_AwContents_onNewPicture(env, obj.obj());
}

base::android::ScopedJavaLocalRef<jbyteArray>
    AwContents::GetCertificate(JNIEnv* env,
                               jobject obj) {
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
  render_view_host_ext_->RequestNewHitTestDataAt(x, y);
}

void AwContents::UpdateLastHitTestData(JNIEnv* env, jobject obj) {
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
  browser_view_renderer_->OnSizeChanged(w, h);
}

void AwContents::SetVisibility(JNIEnv* env, jobject obj, bool visible) {
  browser_view_renderer_->OnVisibilityChanged(visible);
}

void AwContents::OnAttachedToWindow(JNIEnv* env, jobject obj, int w, int h) {
  browser_view_renderer_->OnAttachedToWindow(w, h);
}

void AwContents::OnDetachedFromWindow(JNIEnv* env, jobject obj) {
  browser_view_renderer_->OnDetachedFromWindow();
}

base::android::ScopedJavaLocalRef<jbyteArray>
AwContents::GetOpaqueState(JNIEnv* env, jobject obj) {
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
                        jint clip_left,
                        jint clip_top,
                        jint clip_right,
                        jint clip_bottom) {
  return browser_view_renderer_->OnDraw(canvas,
                                        is_hardware_accelerated,
                                        gfx::Vector2d(scroll_x, scroll_y),
                                        gfx::Rect(clip_left,
                                                  clip_top,
                                                  clip_right - clip_left,
                                                  clip_bottom - clip_top));
}

void AwContents::SetPendingWebContentsForPopup(
    scoped_ptr<content::WebContents> pending) {
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
  web_contents_->FocusThroughTabTraversal(false);
}

jint AwContents::ReleasePopupAwContents(JNIEnv* env, jobject obj) {
  return reinterpret_cast<jint>(pending_contents_.release());
}

gfx::Point AwContents::GetLocationOnScreen() {
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

void AwContents::ScrollContainerViewTo(gfx::Vector2d new_value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_scrollContainerViewTo(
      env, obj.obj(), new_value.x(), new_value.y());
}


void AwContents::SetDipScale(JNIEnv* env, jobject obj, jfloat dipScale) {
  browser_view_renderer_->SetDipScale(dipScale);
}

void AwContents::ScrollTo(JNIEnv* env, jobject obj, jint xPix, jint yPix) {
  browser_view_renderer_->ScrollTo(gfx::Vector2d(xPix, yPix));
}

void AwContents::OnPageScaleFactorChanged(float page_scale_factor) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_onPageScaleFactorChanged(env, obj.obj(), page_scale_factor);
}

ScopedJavaLocalRef<jobject> AwContents::CapturePicture(JNIEnv* env,
                                                       jobject obj) {
  return browser_view_renderer_->CapturePicture();
}

void AwContents::EnableOnNewPicture(JNIEnv* env,
                                    jobject obj,
                                    jboolean enabled) {
  browser_view_renderer_->EnableOnNewPicture(enabled);
}

}  // namespace android_webview
