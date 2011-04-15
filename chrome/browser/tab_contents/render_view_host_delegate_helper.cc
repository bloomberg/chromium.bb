// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"

#include <string>

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/background_contents_service_factory.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/gpu_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/user_style_sheet_watcher.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_widget_fullscreen_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/webui/web_ui.h"

RenderViewHostDelegateViewHelper::RenderViewHostDelegateViewHelper() {}

RenderViewHostDelegateViewHelper::~RenderViewHostDelegateViewHelper() {}

BackgroundContents*
RenderViewHostDelegateViewHelper::MaybeCreateBackgroundContents(
    int route_id,
    Profile* profile,
    SiteInstance* site,
    const GURL& opener_url,
    const string16& frame_name) {
  ExtensionService* extensions_service = profile->GetExtensionService();

  if (!opener_url.is_valid() ||
      frame_name.empty() ||
      !extensions_service ||
      !extensions_service->is_ready())
    return NULL;

  // Only hosted apps have web extents, so this ensures that only hosted apps
  // can create BackgroundContents. We don't have to check for background
  // permission as that is checked in RenderMessageFilter when the CreateWindow
  // message is processed.
  const Extension* extension =
      extensions_service->GetExtensionByWebExtent(opener_url);
  if (!extension)
    return NULL;

  // If the extension manifest specifies a background page, then don't allow one
  // to be created here.
  if (extension->background_url().is_valid())
    return NULL;

  // Only allow a single background contents per app.
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile);
  if (!service || service->GetAppBackgroundContents(
          ASCIIToUTF16(extension->id())))
    return NULL;

  // Ensure that we're trying to open this from the extension's process.
  ExtensionProcessManager* process_manager =
      profile->GetExtensionProcessManager();
  if (!site->GetProcess() || !process_manager ||
      site->GetProcess() != process_manager->GetExtensionProcess(opener_url))
    return NULL;

  // Passed all the checks, so this should be created as a BackgroundContents.
  return service->CreateBackgroundContents(site, route_id, profile, frame_name,
                                           ASCIIToUTF16(extension->id()));
}

TabContents* RenderViewHostDelegateViewHelper::CreateNewWindow(
    int route_id,
    Profile* profile,
    SiteInstance* site,
    WebUI::TypeID webui_type,
    RenderViewHostDelegate* opener,
    WindowContainerType window_container_type,
    const string16& frame_name) {
  if (window_container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    BackgroundContents* contents = MaybeCreateBackgroundContents(
        route_id,
        profile,
        site,
        opener->GetURL(),
        frame_name);
    if (contents) {
      pending_contents_[route_id] = contents->render_view_host();
      return NULL;
    }
  }

  // Create the new web contents. This will automatically create the new
  // TabContentsView. In the future, we may want to create the view separately.
  TabContents* new_contents =
      new TabContents(profile,
                      site,
                      route_id,
                      opener->GetAsTabContents(),
                      NULL);
  new_contents->set_opener_web_ui_type(webui_type);
  TabContentsView* new_view = new_contents->view();

  // TODO(brettw) it seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreateViewForWidget(new_contents->render_view_host());

  // Save the created window associated with the route so we can show it later.
  pending_contents_[route_id] = new_contents->render_view_host();
  return new_contents;
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::CreateNewWidget(
    int route_id, WebKit::WebPopupType popup_type, RenderProcessHost* process) {
  RenderWidgetHost* widget_host =
      new RenderWidgetHost(process, route_id);
  RenderWidgetHostView* widget_view =
      RenderWidgetHostView::CreateViewForWidget(widget_host);
  // Popups should not get activated.
  widget_view->set_popup_type(popup_type);
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

RenderWidgetHostView*
RenderViewHostDelegateViewHelper::CreateNewFullscreenWidget(
    int route_id, RenderProcessHost* process) {
  RenderWidgetFullscreenHost* fullscreen_widget_host =
      new RenderWidgetFullscreenHost(process, route_id);
  RenderWidgetHostView* widget_view =
      RenderWidgetHostView::CreateViewForWidget(fullscreen_widget_host);
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

TabContents* RenderViewHostDelegateViewHelper::GetCreatedWindow(int route_id) {
  PendingContents::iterator iter = pending_contents_.find(route_id);
  if (iter == pending_contents_.end()) {
    DCHECK(false);
    return NULL;
  }

  RenderViewHost* new_rvh = iter->second;
  pending_contents_.erase(route_id);

  // The renderer crashed or it is a TabContents and has no view.
  if (!new_rvh->process()->HasConnection() ||
      (new_rvh->delegate()->GetAsTabContents() && !new_rvh->view()))
    return NULL;

  // TODO(brettw) this seems bogus to reach into here and initialize the host.
  new_rvh->Init();
  return new_rvh->delegate()->GetAsTabContents();
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::GetCreatedWidget(
    int route_id) {
  PendingWidgetViews::iterator iter = pending_widget_views_.find(route_id);
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return NULL;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(route_id);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->process()->HasConnection()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return NULL;
  }

  return widget_host_view;
}

void RenderViewHostDelegateViewHelper::RenderWidgetHostDestroyed(
    RenderWidgetHost* host) {
  for (PendingWidgetViews::iterator i = pending_widget_views_.begin();
       i != pending_widget_views_.end(); ++i) {
    if (host->view() == i->second) {
      pending_widget_views_.erase(i);
      return;
    }
  }
}

bool RenderViewHostDelegateHelper::gpu_enabled_ = true;

// static
WebPreferences RenderViewHostDelegateHelper::GetWebkitPrefs(
    Profile* profile, bool is_web_ui) {
  PrefService* prefs = profile->GetPrefs();
  WebPreferences web_prefs;

  web_prefs.standard_font_family =
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitStandardFontFamily));
  web_prefs.fixed_font_family =
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitFixedFontFamily));
  web_prefs.serif_font_family =
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitSerifFontFamily));
  web_prefs.sans_serif_font_family =
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitSansSerifFontFamily));
  web_prefs.cursive_font_family =
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitCursiveFontFamily));
  web_prefs.fantasy_font_family =
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitFantasyFontFamily));

  web_prefs.default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs.default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs.minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs.minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web_prefs.default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs.javascript_can_open_windows_automatically =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
  web_prefs.dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs.shrinks_standalone_images_to_fit =
      prefs->GetBoolean(prefs::kWebKitShrinksStandaloneImagesToFit);
  const DictionaryValue* inspector_settings =
      prefs->GetDictionary(prefs::kWebKitInspectorSettings);
  if (inspector_settings) {
    for (DictionaryValue::key_iterator iter(inspector_settings->begin_keys());
         iter != inspector_settings->end_keys(); ++iter) {
      std::string value;
      if (inspector_settings->GetStringWithoutPathExpansion(*iter, &value))
          web_prefs.inspector_settings.push_back(
              std::make_pair(*iter, value));
    }
  }
  web_prefs.tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  {  // Command line switches are used for preferences with no user interface.
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    web_prefs.developer_extras_enabled =
        !command_line.HasSwitch(switches::kDisableDevTools);
    web_prefs.javascript_enabled =
        !command_line.HasSwitch(switches::kDisableJavaScript) &&
        prefs->GetBoolean(prefs::kWebKitJavascriptEnabled);
    web_prefs.web_security_enabled =
        !command_line.HasSwitch(switches::kDisableWebSecurity) &&
        prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled);
    web_prefs.plugins_enabled =
        !command_line.HasSwitch(switches::kDisablePlugins) &&
        prefs->GetBoolean(prefs::kWebKitPluginsEnabled);
    web_prefs.java_enabled =
        !command_line.HasSwitch(switches::kDisableJava) &&
        prefs->GetBoolean(prefs::kWebKitJavaEnabled);
    web_prefs.loads_images_automatically =
        prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);
    web_prefs.uses_page_cache =
        command_line.HasSwitch(switches::kEnableFastback);
    web_prefs.remote_fonts_enabled =
        !command_line.HasSwitch(switches::kDisableRemoteFonts);
    web_prefs.xss_auditor_enabled =
        !command_line.HasSwitch(switches::kDisableXSSAuditor);
    web_prefs.application_cache_enabled =
        !command_line.HasSwitch(switches::kDisableApplicationCache);

    web_prefs.local_storage_enabled =
        !command_line.HasSwitch(switches::kDisableLocalStorage);
    web_prefs.databases_enabled =
        !command_line.HasSwitch(switches::kDisableDatabases);
    web_prefs.webaudio_enabled =
        command_line.HasSwitch(switches::kEnableWebAudio);
    web_prefs.experimental_webgl_enabled =
        gpu_enabled() &&
        !command_line.HasSwitch(switches::kDisable3DAPIs) &&
        !command_line.HasSwitch(switches::kDisableExperimentalWebGL);
    web_prefs.gl_multisampling_enabled =
        !command_line.HasSwitch(switches::kDisableGLMultisampling);
    web_prefs.site_specific_quirks_enabled =
        !command_line.HasSwitch(switches::kDisableSiteSpecificQuirks);
    web_prefs.allow_file_access_from_file_urls =
        command_line.HasSwitch(switches::kAllowFileAccessFromFiles);
    web_prefs.show_composited_layer_borders =
        command_line.HasSwitch(switches::kShowCompositedLayerBorders);
    web_prefs.show_composited_layer_tree =
        command_line.HasSwitch(switches::kShowCompositedLayerTree);
    web_prefs.show_fps_counter =
        command_line.HasSwitch(switches::kShowFPSCounter);
    web_prefs.accelerated_compositing_enabled =
        gpu_enabled() &&
        !command_line.HasSwitch(switches::kDisableAcceleratedCompositing);
    web_prefs.force_compositing_mode =
        command_line.HasSwitch(switches::kForceCompositingMode);
    web_prefs.accelerated_2d_canvas_enabled =
        gpu_enabled() &&
        command_line.HasSwitch(switches::kEnableAccelerated2dCanvas);
    web_prefs.accelerated_drawing_enabled =
        gpu_enabled() &&
        command_line.HasSwitch(switches::kEnableAcceleratedDrawing);
    web_prefs.accelerated_layers_enabled =
        !command_line.HasSwitch(switches::kDisableAcceleratedLayers);
    web_prefs.composite_to_texture_enabled =
        command_line.HasSwitch(switches::kEnableCompositeToTexture);
    web_prefs.accelerated_plugins_enabled =
        command_line.HasSwitch(switches::kEnableAcceleratedPlugins);
    web_prefs.accelerated_video_enabled =
        !command_line.HasSwitch(switches::kDisableAcceleratedVideo);
    web_prefs.memory_info_enabled =
        command_line.HasSwitch(switches::kEnableMemoryInfo);
    web_prefs.interactive_form_validation_enabled =
        !command_line.HasSwitch(switches::kDisableInteractiveFormValidation);
    web_prefs.fullscreen_enabled =
        command_line.HasSwitch(switches::kEnableFullScreen);

    // The user stylesheet watcher may not exist in a testing profile.
    if (profile->GetUserStyleSheetWatcher()) {
      web_prefs.user_style_sheet_enabled = true;
      web_prefs.user_style_sheet_location =
          profile->GetUserStyleSheetWatcher()->user_style_sheet();
    } else {
      web_prefs.user_style_sheet_enabled = false;
    }
  }

  {  // Certain GPU features might have been blacklisted.
    GpuDataManager* gpu_data_manager = GpuDataManager::GetInstance();
    DCHECK(gpu_data_manager);
    uint32 blacklist_flags = gpu_data_manager->GetGpuFeatureFlags().flags();
    if (blacklist_flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing)
      web_prefs.accelerated_compositing_enabled = false;
    if (blacklist_flags & GpuFeatureFlags::kGpuFeatureWebgl)
      web_prefs.experimental_webgl_enabled = false;
    if (blacklist_flags & GpuFeatureFlags::kGpuFeatureMultisampling)
      web_prefs.gl_multisampling_enabled = false;
  }

  web_prefs.uses_universal_detector =
      prefs->GetBoolean(prefs::kWebKitUsesUniversalDetector);
  web_prefs.text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);
  web_prefs.hyperlink_auditing_enabled =
      prefs->GetBoolean(prefs::kEnableHyperlinkAuditing);

  // Make sure we will set the default_encoding with canonical encoding name.
  web_prefs.default_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByAliasName(
          web_prefs.default_encoding);
  if (web_prefs.default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web_prefs.default_encoding = prefs->GetString(prefs::kDefaultCharset);
  }
  DCHECK(!web_prefs.default_encoding.empty());

  if (is_web_ui) {
    web_prefs.loads_images_automatically = true;
    web_prefs.javascript_enabled = true;
  }

  return web_prefs;
}

void RenderViewHostDelegateHelper::UpdateInspectorSetting(
    Profile* profile, const std::string& key, const std::string& value) {
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kWebKitInspectorSettings);
  DictionaryValue* inspector_settings = update.Get();
  inspector_settings->SetWithoutPathExpansion(key,
                                              Value::CreateStringValue(value));
}

void RenderViewHostDelegateHelper::ClearInspectorSettings(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kWebKitInspectorSettings);
}
