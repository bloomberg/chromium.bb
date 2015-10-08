// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/browser/android/offline_pages/offline_page_bridge.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/content_settings/popup_blocked_infobar_delegate.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/navigation_interception/navigation_params.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/top_controls_state.h"
#include "jni/Tab_jni.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaByteArray;
using content::BrowserThread;
using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;
using navigation_interception::InterceptNavigationDelegate;
using navigation_interception::NavigationParams;

namespace {

const int kImageSearchThumbnailMinSize = 300 * 300;
const int kImageSearchThumbnailMaxWidth = 600;
const int kImageSearchThumbnailMaxHeight = 600;

}  // namespace

TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(web_contents);
  if (!core_tab_helper)
    return NULL;

  CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
  if (!core_delegate)
    return NULL;

  return static_cast<TabAndroid*>(core_delegate);
}

TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, jobject obj) {
  return reinterpret_cast<TabAndroid*>(Java_Tab_getNativePtr(env, obj));
}

void TabAndroid::AttachTabHelpers(content::WebContents* web_contents) {
  DCHECK(web_contents);

  TabHelpers::AttachTabHelpers(web_contents);
}

TabAndroid::TabAndroid(JNIEnv* env, jobject obj)
    : weak_java_tab_(env, obj),
      content_layer_(cc::Layer::Create(content::Compositor::LayerSettings())),
      tab_content_manager_(NULL),
      synced_tab_delegate_(new browser_sync::SyncedTabDelegateAndroid(this)) {
  Java_Tab_setNativePtr(env, obj, reinterpret_cast<intptr_t>(this));
}

TabAndroid::~TabAndroid() {
  GetContentLayer()->RemoveAllChildren();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_clearNativePtr(env, weak_java_tab_.get(env).obj());
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return weak_java_tab_.get(env);
}

scoped_refptr<cc::Layer> TabAndroid::GetContentLayer() const {
  return content_layer_;
}

int TabAndroid::GetAndroidId() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_getId(env, weak_java_tab_.get(env).obj());
}

int TabAndroid::GetSyncId() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_getSyncId(env, weak_java_tab_.get(env).obj());
}

base::string16 TabAndroid::GetTitle() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF16(
      Java_Tab_getTitle(env, weak_java_tab_.get(env).obj()));
}

GURL TabAndroid::GetURL() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return GURL(base::android::ConvertJavaStringToUTF8(
      Java_Tab_getUrl(env, weak_java_tab_.get(env).obj())));
}

bool TabAndroid::LoadIfNeeded() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_loadIfNeeded(env, weak_java_tab_.get(env).obj());
}

content::ContentViewCore* TabAndroid::GetContentViewCore() const {
  if (!web_contents())
    return NULL;

  return content::ContentViewCore::FromWebContents(web_contents());
}

Profile* TabAndroid::GetProfile() const {
  if (!web_contents())
    return NULL;

  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

browser_sync::SyncedTabDelegate* TabAndroid::GetSyncedTabDelegate() const {
  return synced_tab_delegate_.get();
}

void TabAndroid::SetWindowSessionID(SessionID::id_type window_id) {
  session_window_id_.set_id(window_id);

  if (!web_contents())
    return;

  SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents());
  session_tab_helper->SetWindowID(session_window_id_);
}

void TabAndroid::SetSyncId(int sync_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_setSyncId(env, weak_java_tab_.get(env).obj(), sync_id);
}

void TabAndroid::HandlePopupNavigation(chrome::NavigateParams* params) {
  DCHECK(params->source_contents == web_contents());
  DCHECK(params->target_contents == NULL ||
         params->target_contents == web_contents());

  WindowOpenDisposition disposition = params->disposition;
  const GURL& url = params->url;

  if (disposition == NEW_POPUP ||
      disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == NEW_WINDOW ||
      disposition == OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> jobj = weak_java_tab_.get(env);
    ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(env, url.spec()));
    ScopedJavaLocalRef<jstring> jheaders(
        ConvertUTF8ToJavaString(env, params->extra_headers));
    ScopedJavaLocalRef<jbyteArray> jpost_data;
    if (params->uses_post &&
        params->browser_initiated_post_data.get() &&
        params->browser_initiated_post_data.get()->size()) {
      jpost_data = ToJavaByteArray(
          env,
          reinterpret_cast<const uint8*>(
              params->browser_initiated_post_data.get()->front()),
          params->browser_initiated_post_data.get()->size());
    }
    Java_Tab_openNewTab(env,
                        jobj.obj(),
                        jurl.obj(),
                        jheaders.obj(),
                        jpost_data.obj(),
                        disposition,
                        params->created_with_opener,
                        params->is_renderer_initiated);
  } else {
    NOTIMPLEMENTED();
  }
}

bool TabAndroid::HasPrerenderedUrl(GURL gurl) {
  prerender::PrerenderManager* prerender_manager = GetPrerenderManager();
  if (!prerender_manager)
    return false;

  std::vector<content::WebContents*> contents =
      prerender_manager->GetAllPrerenderingContents();
  prerender::PrerenderContents* prerender_contents;
  for (size_t i = 0; i < contents.size(); ++i) {
    prerender_contents = prerender_manager->
        GetPrerenderContents(contents.at(i));
    if (prerender_contents->prerender_url() == gurl &&
        prerender_contents->has_finished_loading()) {
      return true;
    }
  }
  return false;
}

void TabAndroid::MakeLoadURLParams(
    chrome::NavigateParams* params,
    NavigationController::LoadURLParams* load_url_params) {
  load_url_params->referrer = params->referrer;
  load_url_params->frame_tree_node_id = params->frame_tree_node_id;
  load_url_params->redirect_chain = params->redirect_chain;
  load_url_params->transition_type = params->transition;
  load_url_params->extra_headers = params->extra_headers;
  load_url_params->should_replace_current_entry =
      params->should_replace_current_entry;

  if (params->transferred_global_request_id != GlobalRequestID()) {
    load_url_params->transferred_global_request_id =
        params->transferred_global_request_id;
  }
  load_url_params->is_renderer_initiated = params->is_renderer_initiated;

  // Only allows the browser-initiated navigation to use POST.
  if (params->uses_post && !params->is_renderer_initiated) {
    load_url_params->load_type =
        NavigationController::LOAD_TYPE_BROWSER_INITIATED_HTTP_POST;
    load_url_params->browser_initiated_post_data =
        params->browser_initiated_post_data;
  }
}

void TabAndroid::SwapTabContents(content::WebContents* old_contents,
                                 content::WebContents* new_contents,
                                 bool did_start_load,
                                 bool did_finish_load) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_swapWebContents(
      env,
      weak_java_tab_.get(env).obj(),
      new_contents->GetJavaWebContents().obj(),
      did_start_load,
      did_finish_load);
}

void TabAndroid::DefaultSearchProviderChanged(
    bool google_base_url_domain_changed) {
  // TODO(kmadhusu): Move this function definition to a common place and update
  // BrowserInstantController::DefaultSearchProviderChanged to use the same.
  if (!web_contents())
    return;

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  if (!instant_service)
    return;

  // Send new search URLs to the renderer.
  content::RenderProcessHost* rph = web_contents()->GetRenderProcessHost();
  instant_service->SendSearchURLsToRenderer(rph);

  // Reload the contents to ensure that it gets assigned to a non-previledged
  // renderer.
  if (!instant_service->IsInstantProcess(rph->GetID()))
    return;
  web_contents()->GetController().Reload(false);

  // As the reload was not triggered by the user we don't want to close any
  // infobars. We have to tell the InfoBarService after the reload, otherwise it
  // would ignore this call when
  // WebContentsObserver::DidStartNavigationToPendingEntry is invoked.
  InfoBarService::FromWebContents(web_contents())->set_ignore_next_reload();
}

void TabAndroid::OnWebContentsInstantSupportDisabled(
    const content::WebContents* contents) {
  DCHECK(contents);
  if (web_contents() != contents)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_onWebContentsInstantSupportDisabled(env,
                                               weak_java_tab_.get(env).obj());
}

void TabAndroid::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  JNIEnv* env = base::android::AttachCurrentThread();
  switch (type) {
    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      TabSpecificContentSettings* settings =
          TabSpecificContentSettings::FromWebContents(web_contents());
      if (!settings->IsBlockageIndicated(CONTENT_SETTINGS_TYPE_POPUPS)) {
        // TODO(dfalcantara): Create an InfoBarDelegate to keep the
        // PopupBlockedInfoBar logic native-side instead of straddling the JNI
        // boundary.
        int num_popups = 0;
        PopupBlockerTabHelper* popup_blocker_helper =
            PopupBlockerTabHelper::FromWebContents(web_contents());
        if (popup_blocker_helper)
          num_popups = popup_blocker_helper->GetBlockedPopupsCount();

        if (num_popups > 0)
          PopupBlockedInfoBarDelegate::Create(web_contents(), num_popups);

        settings->SetBlockageHasBeenIndicated(CONTENT_SETTINGS_TYPE_POPUPS);
      }
      break;
    }
    case content::NOTIFICATION_NAV_ENTRY_CHANGED:
      Java_Tab_onNavEntryChanged(env, weak_java_tab_.get(env).obj());
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

void TabAndroid::OnFaviconAvailable(const gfx::Image& image) {
  SkBitmap favicon = image.AsImageSkia().GetRepresentation(1.0f).sk_bitmap();
  if (favicon.empty())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_onFaviconAvailable(env, weak_java_tab_.get(env).obj(),
                              gfx::ConvertToJavaBitmap(&favicon).obj());
}

void TabAndroid::OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                                  bool icon_url_changed) {
}

void TabAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void TabAndroid::InitWebContents(JNIEnv* env,
                                 jobject obj,
                                 jboolean incognito,
                                 jobject jcontent_view_core,
                                 jobject jweb_contents_delegate,
                                 jobject jcontext_menu_populator) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  web_contents_.reset(content_view_core->GetWebContents());
  AttachTabHelpers(web_contents_.get());

  SetWindowSessionID(session_window_id_.id());

  session_tab_id_.set_id(
      SessionTabHelper::FromWebContents(web_contents())->session_id().id());
  ContextMenuHelper::FromWebContents(web_contents())->SetPopulator(
      jcontext_menu_populator);
  WindowAndroidHelper::FromWebContents(web_contents())->
      SetWindowAndroid(content_view_core->GetWindowAndroid());
  CoreTabHelper::FromWebContents(web_contents())->set_delegate(this);
  SearchTabHelper::FromWebContents(web_contents())->set_delegate(this);
  web_contents_delegate_.reset(
      new chrome::android::TabWebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_delegate_->LoadProgressChanged(web_contents(), 0);
  web_contents()->SetDelegate(web_contents_delegate_.get());

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::Source<content::NavigationController>(
           &web_contents()->GetController()));

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());

  if (favicon_driver)
    favicon_driver->AddObserver(this);

  synced_tab_delegate_->SetWebContents(web_contents());

  // Verify that the WebContents this tab represents matches the expected
  // off the record state.
  CHECK_EQ(GetProfile()->IsOffTheRecord(), incognito);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  if (instant_service)
    instant_service->AddObserver(this);

  content_layer_->InsertChild(content_view_core->GetLayer(), 0);
}

void TabAndroid::DestroyWebContents(JNIEnv* env,
                                    jobject obj,
                                    jboolean delete_native) {
  DCHECK(web_contents());

  content::ContentViewCore* content_view_core = GetContentViewCore();
  if (content_view_core)
    content_view_core->GetLayer()->RemoveFromParent();

  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Remove(
      this,
      content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::Source<content::NavigationController>(
           &web_contents()->GetController()));

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());

  if (favicon_driver)
    favicon_driver->RemoveObserver(this);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  if (instant_service)
    instant_service->RemoveObserver(this);

  web_contents()->SetDelegate(NULL);

  if (delete_native) {
    // Terminate the renderer process if this is the last tab.
    // If there's no unload listener, FastShutdownForPageCount kills the
    // renderer process. Otherwise, we go with the slow path where renderer
    // process shuts down itself when ref count becomes 0.
    // This helps the render process exit quickly which avoids some issues
    // during shutdown. See https://codereview.chromium.org/146693011/
    // and http://crbug.com/338709 for details.
    content::RenderProcessHost* process =
        web_contents()->GetRenderProcessHost();
    if (process)
      process->FastShutdownForPageCount(1);

    web_contents_.reset();
    synced_tab_delegate_->ResetWebContents();
  } else {
    // Release the WebContents so it does not get deleted by the scoped_ptr.
    ignore_result(web_contents_.release());
  }
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetProfileAndroid(
    JNIEnv* env,
    jobject obj) {
  Profile* profile = GetProfile();
  if (!profile)
    return base::android::ScopedJavaLocalRef<jobject>();
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (!profile_android)
    return base::android::ScopedJavaLocalRef<jobject>();

  return profile_android->GetJavaObject();
}

TabAndroid::TabLoadStatus TabAndroid::LoadUrl(
    JNIEnv* env,
    jobject obj,
    jstring url,
    jstring j_extra_headers,
    jbyteArray j_post_data,
    jint page_transition,
    jstring j_referrer_url,
    jint referrer_policy,
    jboolean is_renderer_initiated,
    jboolean should_replace_current_entry,
    jlong intent_received_timestamp,
    jboolean has_user_gesture) {
  if (!web_contents())
    return PAGE_LOAD_FAILED;

  GURL gurl(base::android::ConvertJavaStringToUTF8(env, url));
  if (gurl.is_empty())
    return PAGE_LOAD_FAILED;

  // If the page was prerendered, use it.
  // Note in incognito mode, we don't have a PrerenderManager.

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(GetProfile());
  if (prerender_manager) {
    bool prefetched_page_loaded = HasPrerenderedUrl(gurl);
    // Getting the load status before MaybeUsePrerenderedPage() b/c it resets.
    chrome::NavigateParams params(web_contents());
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(GetProfile());
    if (prerenderer) {
      const base::string16& search_terms =
          search::ExtractSearchTermsFromURL(GetProfile(), gurl);
      if (!search_terms.empty() &&
          prerenderer->CanCommitQuery(web_contents_.get(), search_terms)) {
        EmbeddedSearchRequestParams request_params(gurl);
        prerenderer->Commit(search_terms, request_params);

        if (prerenderer->UsePrerenderedPage(gurl, &params))
          return FULL_PRERENDERED_PAGE_LOAD;
      }
      prerenderer->Cancel();
    }
    if (prerender_manager->MaybeUsePrerenderedPage(gurl, &params)) {
      return prefetched_page_loaded ?
          FULL_PRERENDERED_PAGE_LOAD : PARTIAL_PRERENDERED_PAGE_LOAD;
    }
  }

  GURL fixed_url(
      url_formatter::FixupURL(gurl.possibly_invalid_spec(), std::string()));
  if (!fixed_url.is_valid())
    return PAGE_LOAD_FAILED;

  if (!HandleNonNavigationAboutURL(fixed_url)) {
    // Record UMA "ShowHistory" here. That way it'll pick up both user
    // typing chrome://history as well as selecting from the drop down menu.
    if (fixed_url.spec() == chrome::kChromeUIHistoryURL) {
      content::RecordAction(base::UserMetricsAction("ShowHistory"));
    }

    content::NavigationController::LoadURLParams load_params(fixed_url);
    if (j_extra_headers) {
      load_params.extra_headers = base::android::ConvertJavaStringToUTF8(
          env,
          j_extra_headers);
    }
    if (j_post_data) {
      load_params.load_type =
          content::NavigationController::LOAD_TYPE_BROWSER_INITIATED_HTTP_POST;
      std::vector<uint8> post_data;
      base::android::JavaByteArrayToByteVector(env, j_post_data, &post_data);
      load_params.browser_initiated_post_data =
          base::RefCountedBytes::TakeVector(&post_data);
    }
    load_params.transition_type =
        ui::PageTransitionFromInt(page_transition);
    if (j_referrer_url) {
      load_params.referrer = content::Referrer(
          GURL(base::android::ConvertJavaStringToUTF8(env, j_referrer_url)),
          static_cast<blink::WebReferrerPolicy>(referrer_policy));
    }
    const base::string16 search_terms =
        search::ExtractSearchTermsFromURL(GetProfile(), gurl);
    SearchTabHelper* search_tab_helper =
        SearchTabHelper::FromWebContents(web_contents_.get());
    if (!search_terms.empty() && search_tab_helper &&
        search_tab_helper->SupportsInstant()) {
      EmbeddedSearchRequestParams request_params(gurl);
      search_tab_helper->Submit(search_terms, request_params);
      return DEFAULT_PAGE_LOAD;
    }
    load_params.is_renderer_initiated = is_renderer_initiated;
    load_params.should_replace_current_entry = should_replace_current_entry;
    load_params.intent_received_timestamp = intent_received_timestamp;
    load_params.has_user_gesture = has_user_gesture;
    web_contents()->GetController().LoadURLWithParams(load_params);
  }
  return DEFAULT_PAGE_LOAD;
}

void TabAndroid::SetActiveNavigationEntryTitleForUrl(JNIEnv* env,
                                                     jobject obj,
                                                     jstring jurl,
                                                     jstring jtitle) {
  DCHECK(web_contents());

  base::string16 title;
  if (jtitle)
    title = base::android::ConvertJavaStringToUTF16(env, jtitle);

  std::string url;
  if (jurl)
    url = base::android::ConvertJavaStringToUTF8(env, jurl);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (entry && url == entry->GetVirtualURL().spec())
    entry->SetTitle(title);
}

bool TabAndroid::Print(JNIEnv* env, jobject obj) {
  if (!web_contents())
    return false;

  printing::PrintViewManagerBasic::CreateForWebContents(web_contents());
  printing::PrintViewManagerBasic* print_view_manager =
      printing::PrintViewManagerBasic::FromWebContents(web_contents());
  if (print_view_manager == NULL)
    return false;

  print_view_manager->PrintNow();
  return true;
}

void TabAndroid::SetPendingPrint() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_setPendingPrint(env, weak_java_tab_.get(env).obj());
}

ScopedJavaLocalRef<jobject> TabAndroid::GetFavicon(JNIEnv* env,
                                                   jobject obj) {
  ScopedJavaLocalRef<jobject> bitmap;
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());

  if (!favicon_driver)
    return bitmap;

  // Always return the default favicon in Android.
  SkBitmap favicon = favicon_driver->GetFavicon().AsBitmap();
  if (!favicon.empty()) {
    gfx::DeviceDisplayInfo device_info;
    const float device_scale_factor = device_info.GetDIPScale();
    int target_size_dip = device_scale_factor * gfx::kFaviconSize;
    if (favicon.width() != target_size_dip ||
        favicon.height() != target_size_dip) {
      favicon =
          skia::ImageOperations::Resize(favicon,
                                        skia::ImageOperations::RESIZE_BEST,
                                        target_size_dip,
                                        target_size_dip);
    }

    bitmap = gfx::ConvertToJavaBitmap(&favicon);
  }
  return bitmap;
}

prerender::PrerenderManager* TabAndroid::GetPrerenderManager() const {
  Profile* profile = GetProfile();
  if (!profile)
    return NULL;
  return prerender::PrerenderManagerFactory::GetForProfile(profile);
}

// static
void TabAndroid::CreateHistoricalTabFromContents(WebContents* web_contents) {
  DCHECK(web_contents);

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service)
    return;

  // Exclude internal pages from being marked as recent when they are closed.
  const GURL& tab_url = web_contents->GetURL();
  if (tab_url.SchemeIs(content::kChromeUIScheme) ||
      tab_url.SchemeIs(chrome::kChromeNativeScheme) ||
      tab_url.SchemeIs(url::kAboutScheme)) {
    return;
  }

  // TODO(jcivelli): is the index important?
  service->CreateHistoricalTab(
      sessions::ContentLiveTab::GetForWebContents(web_contents), -1);
}

void TabAndroid::CreateHistoricalTab(JNIEnv* env, jobject obj) {
  TabAndroid::CreateHistoricalTabFromContents(web_contents());
}

void TabAndroid::UpdateTopControlsState(JNIEnv* env,
                                        jobject obj,
                                        jint constraints,
                                        jint current,
                                        jboolean animate) {
  content::TopControlsState constraints_state =
      static_cast<content::TopControlsState>(constraints);
  content::TopControlsState current_state =
      static_cast<content::TopControlsState>(current);
  WebContents* sender = web_contents();
  sender->Send(new ChromeViewMsg_UpdateTopControlsState(
      sender->GetRoutingID(), constraints_state, current_state, animate));

  if (sender->ShowingInterstitialPage()) {
    content::RenderViewHost* interstitial_view_host =
        sender->GetInterstitialPage()->GetMainFrame()->GetRenderViewHost();
    interstitial_view_host->Send(new ChromeViewMsg_UpdateTopControlsState(
        interstitial_view_host->GetRoutingID(), constraints_state,
        current_state, animate));
  }
}

void TabAndroid::LoadOriginalImage(JNIEnv* env, jobject obj) {
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetFocusedFrame();
  render_frame_host->Send(new ChromeViewMsg_RequestReloadImageForContextNode(
      render_frame_host->GetRoutingID()));
}

void TabAndroid::SearchByImageInNewTabAsync(JNIEnv* env, jobject obj) {
  content::RenderFrameHost* render_frame_host =
        web_contents()->GetMainFrame();
  render_frame_host->Send(
      new ChromeViewMsg_RequestThumbnailForContextNode(
          render_frame_host->GetRoutingID(),
          kImageSearchThumbnailMinSize,
          gfx::Size(kImageSearchThumbnailMaxWidth,
                    kImageSearchThumbnailMaxHeight)));
}

jlong TabAndroid::GetBookmarkId(JNIEnv* env,
                               jobject obj,
                               jboolean only_editable) {
  GURL url = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
      web_contents()->GetURL());
  Profile* profile = GetProfile();

  // If the url points to an offline page, replace it with the original url.
  const offline_pages::OfflinePageItem* offline_page = GetOfflinePage(url);
  if (offline_page)
    url = offline_page->url;

  // Get all the nodes for |url| and sort them by date added.
  std::vector<const bookmarks::BookmarkNode*> nodes;
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile);
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForProfile(profile);
  model->GetNodesByURL(url, &nodes);
  std::sort(nodes.begin(), nodes.end(), &bookmarks::MoreRecentlyAdded);

  // Return the first node matching the search criteria.
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (only_editable && !managed->CanBeEditedByUser(nodes[i]))
      continue;
    return nodes[i]->id();
  }

  return -1;
}

jboolean TabAndroid::HasOfflineCopy(JNIEnv* env, jobject obj) {
  // Offline copy is only saved for a bookmarked page.
  jlong bookmark_id = GetBookmarkId(env, obj, true);
  if (bookmark_id == -1)
    return false;

  offline_pages::OfflinePageModel* offline_page_model =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(
          GetProfile());
  if (!offline_page_model)
    return false;
  const offline_pages::OfflinePageItem* offline_page =
      offline_page_model->GetPageByBookmarkId(bookmark_id);
  return offline_page && !offline_page->file_path.empty();
}

jboolean TabAndroid::IsOfflinePage(JNIEnv* env, jobject obj) {
  GURL url = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
      web_contents()->GetURL());
  return GetOfflinePage(url) != nullptr;
}

ScopedJavaLocalRef<jstring> TabAndroid::GetOfflinePageOriginalUrl(JNIEnv* env,
                                                                  jobject obj) {
  GURL url = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
      web_contents()->GetURL());
  const offline_pages::OfflinePageItem* offline_page = GetOfflinePage(url);
  if (offline_page == nullptr)
    return ScopedJavaLocalRef<jstring>();

  return ScopedJavaLocalRef<jstring>(
      ConvertUTF8ToJavaString(env, offline_page->url.spec()));
}

const offline_pages::OfflinePageItem* TabAndroid::GetOfflinePage(
    const GURL& url) const {
  if (!offline_pages::IsOfflinePagesEnabled())
    return nullptr;

  // Note that we first check if the url likely points to an offline page
  // before calling GetPageByOfflineURL in order to avoid unnecessary lookup
  // cost.
  if (!offline_pages::android::OfflinePageBridge::MightBeOfflineURL(url))
    return nullptr;

  offline_pages::OfflinePageModel* offline_page_model =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(
          GetProfile());
  return offline_page_model->GetPageByOfflineURL(url);
}

bool TabAndroid::HasPrerenderedUrl(JNIEnv* env, jobject obj, jstring url) {
  GURL gurl(base::android::ConvertJavaStringToUTF8(env, url));
  return HasPrerenderedUrl(gurl);
}

namespace {

class ChromeInterceptNavigationDelegate : public InterceptNavigationDelegate {
 public:
  ChromeInterceptNavigationDelegate(JNIEnv* env, jobject jdelegate)
      : InterceptNavigationDelegate(env, jdelegate) {}

  bool ShouldIgnoreNavigation(
      const NavigationParams& navigation_params) override {
    NavigationParams chrome_navigation_params(navigation_params);
    chrome_navigation_params.url() =
        GURL(net::EscapeExternalHandlerValue(navigation_params.url().spec()));
    return InterceptNavigationDelegate::ShouldIgnoreNavigation(
        chrome_navigation_params);
  }
};

}  // namespace

void TabAndroid::SetInterceptNavigationDelegate(JNIEnv* env, jobject obj,
                                               jobject delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InterceptNavigationDelegate::Associate(
      web_contents(),
      make_scoped_ptr(new ChromeInterceptNavigationDelegate(env, delegate)));
}

void TabAndroid::AttachToTabContentManager(JNIEnv* env,
                                           jobject obj,
                                           jobject jtab_content_manager) {
  chrome::android::TabContentManager* tab_content_manager =
      chrome::android::TabContentManager::FromJavaObject(jtab_content_manager);
  if (tab_content_manager == tab_content_manager_)
    return;

  if (tab_content_manager_)
    tab_content_manager_->DetachLiveLayer(GetAndroidId(), GetContentLayer());
  tab_content_manager_ = tab_content_manager;
  if (tab_content_manager_)
    tab_content_manager_->AttachLiveLayer(GetAndroidId(), GetContentLayer());
}

void TabAndroid::AttachOverlayContentViewCore(JNIEnv* env,
                                              jobject obj,
                                              jobject jcontent_view_core,
                                              jboolean visible) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);

  content_view_core->GetLayer()->SetHideLayerAndSubtree(!visible);
  content_layer_->AddChild(content_view_core->GetLayer());
}

void TabAndroid::DetachOverlayContentViewCore(JNIEnv* env,
                                              jobject obj,
                                              jobject jcontent_view_core) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);

  if (content_view_core->GetLayer()->parent() == content_layer_)
    content_view_core->GetLayer()->RemoveFromParent();
}

static void Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  TRACE_EVENT0("native", "TabAndroid::Init");
  // This will automatically bind to the Java object and pass ownership there.
  new TabAndroid(env, obj);
}

// static
bool TabAndroid::RegisterTabAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
