// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_tab_helper.h"
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
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar_container.h"
#include "components/url_fixer/url_fixer.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "jni/Tab_jni.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;

namespace {

WebContents* CreateTargetContents(const chrome::NavigateParams& params,
                                  const GURL& url) {
  Profile* profile = params.initiating_profile;

  if (profile->IsOffTheRecord() || params.disposition == OFF_THE_RECORD) {
    profile = profile->GetOffTheRecordProfile();
  }
  WebContents::CreateParams create_params(
      profile, tab_util::GetSiteInstanceForNewTab(profile, url));
  if (params.source_contents) {
    create_params.initial_size =
        params.source_contents->GetContainerBounds().size();
    if (params.should_set_opener)
      create_params.opener = params.source_contents;
  }
  if (params.disposition == NEW_BACKGROUND_TAB)
    create_params.initially_hidden = true;

  WebContents* target_contents = WebContents::Create(create_params);

  return target_contents;
}

bool MaybeSwapWithPrerender(const GURL& url, chrome::NavigateParams* params) {
  Profile* profile =
      Profile::FromBrowserContext(params->target_contents->GetBrowserContext());

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  if (!prerender_manager)
    return false;
  return prerender_manager->MaybeUsePrerenderedPage(url, params);
}

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
      synced_tab_delegate_(new browser_sync::SyncedTabDelegateAndroid(this)) {
  Java_Tab_setNativePtr(env, obj, reinterpret_cast<intptr_t>(this));
}

TabAndroid::~TabAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_clearNativePtr(env, weak_java_tab_.get(env).obj());
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return weak_java_tab_.get(env);
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
  if (params->disposition != SUPPRESS_OPEN &&
      params->disposition != SAVE_TO_DISK &&
      params->disposition != IGNORE_ACTION) {
    if (!params->url.is_empty()) {
      bool was_blocked = false;
      GURL url(params->url);
      if (params->disposition == CURRENT_TAB) {
        params->target_contents = web_contents_.get();
        if (!MaybeSwapWithPrerender(url, params)) {
          NavigationController::LoadURLParams load_url_params(url);
          MakeLoadURLParams(params, &load_url_params);
          params->target_contents->GetController().LoadURLWithParams(
              load_url_params);
        }
      } else {
        params->target_contents = CreateTargetContents(*params, url);
        NavigationController::LoadURLParams load_url_params(url);
        MakeLoadURLParams(params, &load_url_params);
        params->target_contents->GetController().LoadURLWithParams(
            load_url_params);
        web_contents_delegate_->AddNewContents(params->source_contents,
                                               params->target_contents,
                                               params->disposition,
                                               params->window_bounds,
                                               params->user_gesture,
                                               &was_blocked);
        if (was_blocked)
          params->target_contents = NULL;
      }
    }
  }
}

bool TabAndroid::ShouldWelcomePageLinkToTermsOfService() {
  NOTIMPLEMENTED();
  return false;
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

  // We need to notify the native InfobarContainer so infobars can be swapped.
  InfoBarContainerAndroid* infobar_container =
      reinterpret_cast<InfoBarContainerAndroid*>(
          Java_Tab_getNativeInfoBarContainer(
              env,
              weak_java_tab_.get(env).obj()));
  InfoBarService* new_infobar_service =
      new_contents ? InfoBarService::FromWebContents(new_contents) : NULL;
  infobar_container->ChangeInfoBarManager(new_infobar_service);

  Java_Tab_swapWebContents(
      env,
      weak_java_tab_.get(env).obj(),
      reinterpret_cast<intptr_t>(new_contents),
      did_start_load,
      did_finish_load);
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
    case chrome::NOTIFICATION_FAVICON_UPDATED:
      Java_Tab_onFaviconUpdated(env, weak_java_tab_.get(env).obj());
      break;
    case content::NOTIFICATION_NAV_ENTRY_CHANGED:
      Java_Tab_onNavEntryChanged(env, weak_java_tab_.get(env).obj());
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
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
      new chrome::android::ChromeWebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_delegate_->LoadProgressChanged(web_contents(), 0);
  web_contents()->SetDelegate(web_contents_delegate_.get());

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::Source<content::NavigationController>(
           &web_contents()->GetController()));

  synced_tab_delegate_->SetWebContents(web_contents());

  // Verify that the WebContents this tab represents matches the expected
  // off the record state.
  CHECK_EQ(GetProfile()->IsOffTheRecord(), incognito);
}

void TabAndroid::DestroyWebContents(JNIEnv* env,
                                    jobject obj,
                                    jboolean delete_native) {
  DCHECK(web_contents());

  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Remove(
      this,
      content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::Source<content::NavigationController>(
           &web_contents()->GetController()));

  web_contents()->SetDelegate(NULL);

  if (delete_native) {
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

TabAndroid::TabLoadStatus TabAndroid::LoadUrl(JNIEnv* env,
                                              jobject obj,
                                              jstring url,
                                              jstring j_extra_headers,
                                              jbyteArray j_post_data,
                                              jint page_transition,
                                              jstring j_referrer_url,
                                              jint referrer_policy,
                                              jboolean is_renderer_initiated) {
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
    chrome::NavigateParams params(NULL, web_contents());
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(GetProfile());
    if (prerenderer) {
      const base::string16& search_terms =
          chrome::ExtractSearchTermsFromURL(GetProfile(), gurl);
      if (!search_terms.empty() &&
          prerenderer->CanCommitQuery(web_contents_.get(), search_terms)) {
        prerenderer->Commit(search_terms);

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
      url_fixer::FixupURL(gurl.possibly_invalid_spec(), std::string()));
  if (!fixed_url.is_valid())
    return PAGE_LOAD_FAILED;

  if (!HandleNonNavigationAboutURL(fixed_url)) {
    // Notify the GoogleURLTracker of searches, it might want to change the
    // actual Google site used (for instance when in the UK, google.co.uk, when
    // in the US google.com).
    // Note that this needs to happen before we initiate the navigation as the
    // GoogleURLTracker uses the navigation pending notification to trigger the
    // infobar.
    if (google_util::IsGoogleSearchUrl(fixed_url) &&
        (page_transition & ui::PAGE_TRANSITION_GENERATED)) {
      GoogleURLTracker* tracker =
          GoogleURLTrackerFactory::GetForProfile(GetProfile());
      if (tracker)
        tracker->SearchCommitted();
    }

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
        chrome::ExtractSearchTermsFromURL(GetProfile(), gurl);
    SearchTabHelper* search_tab_helper =
        SearchTabHelper::FromWebContents(web_contents_.get());
    if (!search_terms.empty() && search_tab_helper &&
        search_tab_helper->SupportsInstant()) {
      search_tab_helper->Submit(search_terms);
      return DEFAULT_PAGE_LOAD;
    }
    load_params.is_renderer_initiated = is_renderer_initiated;
    web_contents()->GetController().LoadURLWithParams(load_params);
  }
  return DEFAULT_PAGE_LOAD;
}

ToolbarModel::SecurityLevel TabAndroid::GetSecurityLevel(JNIEnv* env,
                                                         jobject obj) {
  return ToolbarModelImpl::GetSecurityLevelForWebContents(web_contents());
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

ScopedJavaLocalRef<jobject> TabAndroid::GetFavicon(JNIEnv* env, jobject obj) {
  ScopedJavaLocalRef<jobject> bitmap;
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents_.get());

  if (!favicon_tab_helper)
    return bitmap;

  // If the favicon isn't valid, it will return a default bitmap.

  SkBitmap favicon =
      favicon_tab_helper->GetFavicon()
          .AsImageSkia()
          .GetRepresentation(
               ResourceBundle::GetSharedInstance().GetMaxScaleFactor())
          .sk_bitmap();

  if (favicon.empty()) {
    favicon = favicon_tab_helper->GetFavicon().AsBitmap();
  }

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

jboolean TabAndroid::IsFaviconValid(JNIEnv* env, jobject jobj) {
  return web_contents() &&
      FaviconTabHelper::FromWebContents(web_contents())->FaviconIsValid();
}

prerender::PrerenderManager* TabAndroid::GetPrerenderManager() const {
  Profile* profile = GetProfile();
  if (!profile)
    return NULL;
  return prerender::PrerenderManagerFactory::GetForProfile(profile);
}

static void Init(JNIEnv* env, jobject obj) {
  TRACE_EVENT0("native", "TabAndroid::Init");
  // This will automatically bind to the Java object and pass ownership there.
  new TabAndroid(env, obj);
}

bool TabAndroid::RegisterTabAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
