// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_PARAMS_H_
#define CHROME_COMMON_RENDER_MESSAGES_PARAMS_H_
#pragma once

#include <string>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/navigation_gesture.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/common/window_container_type.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_param_traits.h"
#include "media/audio/audio_parameters.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/webplugin.h"

// TODO(erg): Split this file into $1_db_params.h, $1_audio_params.h,
// $1_print_params.h and $1_render_params.h.

namespace net {
class UploadData;
}

// Parameters structure for ViewMsg_Navigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewMsg_Navigate_Params {
  enum NavigationType {
    // Reload the page.
    RELOAD,

    // Reload the page, ignoring any cache entries.
    RELOAD_IGNORING_CACHE,

    // The navigation is the result of session restore and should honor the
    // page's cache policy while restoring form state. This is set to true if
    // restoring a tab/session from the previous session and the previous
    // session did not crash. If this is not set and the page was restored then
    // the page's cache policy is ignored and we load from the cache.
    RESTORE,

    // Speculatively prerendering the page.
    PRERENDER,

    // Navigation type not categorized by the other types.
    NORMAL
  };

  ViewMsg_Navigate_Params();
  ~ViewMsg_Navigate_Params();

  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // ViewHostMsg_FrameNavigate message.
  int32 page_id;

  // If page_id is -1, then pending_history_list_offset will also be -1.
  // Otherwise, it contains the offset into the history list corresponding to
  // the current navigation.
  int pending_history_list_offset;

  // Informs the RenderView of where its current page contents reside in
  // session history and the total size of the session history list.
  int current_history_list_offset;
  int current_history_list_length;

  // The URL to load.
  GURL url;

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  // TODO: consider folding this into extra_headers.
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Opaque history state (received by ViewHostMsg_UpdateState).
  std::string state;

  // Type of navigation.
  NavigationType navigation_type;

  // The time the request was created
  base::Time request_time;

  // Extra headers (separated by \n) to send during the request.
  std::string extra_headers;
};

// Current status of the audio output stream in the browser process. Browser
// sends information about the current playback state and error to the
// renderer process using this type.
struct ViewMsg_AudioStreamState_Params {
  enum State {
    kPlaying,
    kPaused,
    kError
  };

  ViewMsg_AudioStreamState_Params()
      : state(kPlaying) {
  }

  explicit ViewMsg_AudioStreamState_Params(State s)
      : state(s) {
  }

  // Carries the current playback state.
  State state;
};

// The user has completed a find-in-page; this type defines what actions the
// renderer should take next.
struct ViewMsg_StopFinding_Params {
  enum Action {
    kClearSelection,
    kKeepSelection,
    kActivateSelection
  };

  ViewMsg_StopFinding_Params()
      : action(kClearSelection) {
  }

  // The action that should be taken when the find is completed.
  Action action;
};

// The type of OSDD that the renderer is giving to the browser.
struct ViewHostMsg_PageHasOSDD_Type {
  enum Type {
    // The Open Search Description URL was detected automatically.
    AUTODETECTED_PROVIDER,

    // The Open Search Description URL was given by Javascript.
    EXPLICIT_PROVIDER,

    // The Open Search Description URL was given by Javascript to be the new
    // default search engine.
    EXPLICIT_DEFAULT_PROVIDER
  };

  Type type;

  ViewHostMsg_PageHasOSDD_Type() : type(AUTODETECTED_PROVIDER) {
  }

  explicit ViewHostMsg_PageHasOSDD_Type(Type t)
      : type(t) {
  }

  static ViewHostMsg_PageHasOSDD_Type Autodetected() {
    return ViewHostMsg_PageHasOSDD_Type(AUTODETECTED_PROVIDER);
  }

  static ViewHostMsg_PageHasOSDD_Type Explicit() {
    return ViewHostMsg_PageHasOSDD_Type(EXPLICIT_PROVIDER);
  }

  static ViewHostMsg_PageHasOSDD_Type ExplicitDefault() {
    return ViewHostMsg_PageHasOSDD_Type(EXPLICIT_DEFAULT_PROVIDER);
  }
};

// The install state of the search provider (not installed, installed, default).
struct ViewHostMsg_GetSearchProviderInstallState_Params {
  enum State {
    // Equates to an access denied error.
    DENIED = -1,

    // DON'T CHANGE THE VALUES BELOW.
    // All of the following values are manidated by the
    // spec for window.external.IsSearchProviderInstalled.

    // The search provider is not installed.
    NOT_INSTALLED = 0,

    // The search provider is in the user's set but is not
    INSTALLED_BUT_NOT_DEFAULT = 1,

    // The search provider is set as the user's default.
    INSTALLED_AS_DEFAULT = 2
  };
  State state;

  ViewHostMsg_GetSearchProviderInstallState_Params()
      : state(DENIED) {
  }

  explicit ViewHostMsg_GetSearchProviderInstallState_Params(State s)
      : state(s) {
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params Denied() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(DENIED);
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params NotInstalled() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(NOT_INSTALLED);
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params
      InstallButNotDefault() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(
        INSTALLED_BUT_NOT_DEFAULT);
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params InstalledAsDefault() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(
        INSTALLED_AS_DEFAULT);
  }
};

// Parameters structure for ViewHostMsg_FrameNavigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
struct ViewHostMsg_FrameNavigate_Params {
  ViewHostMsg_FrameNavigate_Params();
  ~ViewHostMsg_FrameNavigate_Params();

  // Page ID of this navigation. The renderer creates a new unique page ID
  // anytime a new session history entry is created. This means you'll get new
  // page IDs for user actions, and the old page IDs will be reloaded when
  // iframes are loaded automatically.
  int32 page_id;

  // The frame ID for this navigation. The frame ID uniquely identifies the
  // frame the navigation happened in for a given renderer.
  int64 frame_id;

  // URL of the page being loaded.
  GURL url;

  // URL of the referrer of this load. WebKit generates this based on the
  // source of the event that caused the load.
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Lists the redirects that occurred on the way to the current page. This
  // vector has the same format as reported by the WebDataSource in the glue,
  // with the current page being the last one in the list (so even when
  // there's no redirect, there will be one entry in the list.
  std::vector<GURL> redirects;

  // Set to false if we want to update the session history but not update
  // the browser history.  E.g., on unreachable urls.
  bool should_update_history;

  // See SearchableFormData for a description of these.
  GURL searchable_form_url;
  std::string searchable_form_encoding;

  // See password_form.h.
  webkit_glue::PasswordForm password_form;

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  std::string security_info;

  // The gesture that initiated this navigation.
  NavigationGesture gesture;

  // Contents MIME type of main frame.
  std::string contents_mime_type;

  // True if this was a post request.
  bool is_post;

  // Whether the content of the frame was replaced with some alternate content
  // (this can happen if the resource was insecure).
  bool is_content_filtered;

  // Whether the frame navigation resulted in no change to the documents within
  // the page. For example, the navigation may have just resulted in scrolling
  // to a named anchor.
  bool was_within_same_page;

  // The status code of the HTTP request.
  int http_status_code;
};

// Values that may be OR'd together to form the 'flags' parameter of a
// ViewHostMsg_UpdateRect_Params structure.
struct ViewHostMsg_UpdateRect_Flags {
  enum {
    IS_RESIZE_ACK = 1 << 0,
    IS_RESTORE_ACK = 1 << 1,
    IS_REPAINT_ACK = 1 << 2,
  };
  static bool is_resize_ack(int flags) {
    return (flags & IS_RESIZE_ACK) != 0;
  }
  static bool is_restore_ack(int flags) {
    return (flags & IS_RESTORE_ACK) != 0;
  }
  static bool is_repaint_ack(int flags) {
    return (flags & IS_REPAINT_ACK) != 0;
  }
};

struct ViewHostMsg_UpdateRect_Params {
  ViewHostMsg_UpdateRect_Params();
  ~ViewHostMsg_UpdateRect_Params();

  // The bitmap to be painted into the view at the locations specified by
  // update_rects.
  TransportDIB::Id bitmap;

  // The position and size of the bitmap.
  gfx::Rect bitmap_rect;

  // The scroll offset.  Only one of these can be non-zero, and if they are
  // both zero, then it means there is no scrolling and the scroll_rect is
  // ignored.
  int dx;
  int dy;

  // The rectangular region to scroll.
  gfx::Rect scroll_rect;

  // The scroll offset of the render view.
  gfx::Size scroll_offset;

  // The regions of the bitmap (in view coords) that contain updated pixels.
  // In the case of scrolling, this includes the scroll damage rect.
  std::vector<gfx::Rect> copy_rects;

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress.
  gfx::Size view_size;

  // The area of the RenderView reserved for resize corner when this message
  // was generated.  Reported for the same reason as view_size is.
  gfx::Rect resizer_rect;

  // New window locations for plugin child windows.
  std::vector<webkit::npapi::WebPluginGeometry> plugin_window_moves;

  // The following describes the various bits that may be set in flags:
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK
  //     Indicates that this is a response to a ViewMsg_Resize message.
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_RESTORE_ACK
  //     Indicates that this is a response to a ViewMsg_WasRestored message.
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK
  //     Indicates that this is a response to a ViewMsg_Repaint message.
  //
  // If flags is zero, then this message corresponds to an unsoliticed paint
  // request by the render view.  Any of the above bits may be set in flags,
  // which would indicate that this paint message is an ACK for multiple
  // request messages.
  int flags;
};

// Information on closing a tab. This is used both for ViewMsg_ClosePage, and
// the corresponding ViewHostMsg_ClosePage_ACK.
struct ViewMsg_ClosePage_Params {
  ViewMsg_ClosePage_Params();
  ~ViewMsg_ClosePage_Params();

  // The identifier of the RenderProcessHost for the currently closing view.
  //
  // These first two parameters are technically redundant since they are
  // needed only when processing the ACK message, and the processor
  // theoretically knows both the process and route ID. However, this is
  // difficult to figure out with our current implementation, so this
  // information is duplicate here.
  int closing_process_id;

  // The route identifier for the currently closing RenderView.
  int closing_route_id;

  // True when this close is for the first (closing) tab of a cross-site
  // transition where we switch processes. False indicates the close is for the
  // entire tab.
  //
  // When true, the new_* variables below must be filled in. Otherwise they must
  // both be -1.
  bool for_cross_site_transition;

  // The identifier of the RenderProcessHost for the new view attempting to
  // replace the closing one above. This must be valid when
  // for_cross_site_transition is set, and must be -1 otherwise.
  int new_render_process_host_id;

  // The identifier of the *request* the new view made that is causing the
  // cross-site transition. This is *not* a route_id, but the request that we
  // will resume once the ACK from the closing view has been received. This
  // must be valid when for_cross_site_transition is set, and must be -1
  // otherwise.
  int new_request_id;
};

// Parameters for a resource request.
struct ViewHostMsg_Resource_Request {
  ViewHostMsg_Resource_Request();
  ~ViewHostMsg_Resource_Request();

  // The request method: GET, POST, etc.
  std::string method;

  // The requested URL.
  GURL url;

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy. Leaving it empty may
  // lead to undesired cookie blocking. Third-party cookie blocking can be
  // bypassed by setting first_party_for_cookies = url, but this should ideally
  // only be done if there really is no way to determine the correct value.
  GURL first_party_for_cookies;

  // The referrer to use (may be empty).
  GURL referrer;

  // The origin of the frame that is associated with this request.  This is used
  // to update our insecure content state.
  std::string frame_origin;

  // The origin of the main frame (top-level frame) that is associated with this
  // request.  This is used to update our insecure content state.
  std::string main_frame_origin;

  // Additional HTTP request headers.
  std::string headers;

  // net::URLRequest load flags (0 by default).
  int load_flags;

  // Unique ID of process that originated this request. For normal renderer
  // requests, this will be the ID of the renderer. For plugin requests routed
  // through the renderer, this will be the plugin's ID.
  int origin_child_id;

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  ResourceType::Type resource_type;

  // Used by plugin->browser requests to get the correct net::URLRequestContext.
  uint32 request_context;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kNoHostId.
  int appcache_host_id;

  // Optional upload data (may be null).
  scoped_refptr<net::UploadData> upload_data;

  bool download_to_file;

  // True if the request was user initiated.
  bool has_user_gesture;

  // The following two members are specified if the request is initiated by
  // a plugin like Gears.

  // Contains the id of the host renderer.
  int host_renderer_id;

  // Contains the id of the host render view.
  int host_render_view_id;
};

// Parameters for a render request.
struct ViewMsg_Print_Params {
  ViewMsg_Print_Params();
  ~ViewMsg_Print_Params();

  // Physical size of the page, including non-printable margins,
  // in pixels according to dpi.
  gfx::Size page_size;

  // In pixels according to dpi_x and dpi_y.
  gfx::Size printable_size;

  // The y-offset of the printable area, in pixels according to dpi.
  int margin_top;

  // The x-offset of the printable area, in pixels according to dpi.
  int margin_left;

  // Specifies dots per inch.
  double dpi;

  // Minimum shrink factor. See PrintSettings::min_shrink for more information.
  double min_shrink;

  // Maximum shrink factor. See PrintSettings::max_shrink for more information.
  double max_shrink;

  // Desired apparent dpi on paper.
  int desired_dpi;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Should only print currently selected text.
  bool selection_only;

  // Does the printer support alpha blending?
  bool supports_alpha_blend;

  // Warning: do not compare document_cookie.
  bool Equals(const ViewMsg_Print_Params& rhs) const;

  // Checking if the current params is empty. Just initialized after a memset.
  bool IsEmpty() const;
};

struct ViewMsg_PrintPage_Params {
  ViewMsg_PrintPage_Params();
  ~ViewMsg_PrintPage_Params();

  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // The page number is the indicator of the square that should be rendered
  // according to the layout specified in ViewMsg_Print_Params.
  int page_number;
};

struct ViewMsg_PrintPages_Params {
  ViewMsg_PrintPages_Params();
  ~ViewMsg_PrintPages_Params();

  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // If empty, this means a request to render all the printed pages.
  std::vector<int> pages;
};

// Parameters to describe a rendered document.
struct ViewHostMsg_DidPreviewDocument_Params {
  ViewHostMsg_DidPreviewDocument_Params();
  ~ViewHostMsg_DidPreviewDocument_Params();

  // A shared memory handle to metafile data.
  base::SharedMemoryHandle metafile_data_handle;

  // Size of metafile data.
  uint32 data_size;

  // Cookie for the document to ensure correctness.
  int document_cookie;
};

// Parameters to describe a rendered page.
struct ViewHostMsg_DidPrintPage_Params {
  ViewHostMsg_DidPrintPage_Params();
  ~ViewHostMsg_DidPrintPage_Params();

  // A shared memory handle to the EMF data. This data can be quite large so a
  // memory map needs to be used.
  base::SharedMemoryHandle metafile_data_handle;

  // Size of the metafile data.
  uint32 data_size;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Page number.
  int page_number;

  // Shrink factor used to render this page.
  double actual_shrink;

  // The size of the page the page author specified.
  gfx::Size page_size;

  // The printable area the page author specified.
  gfx::Rect content_area;

  // True if the page has visible overlays.
  bool has_visible_overlays;
};

// Parameters for creating an audio output stream.
struct ViewHostMsg_Audio_CreateStream_Params {
  ViewHostMsg_Audio_CreateStream_Params();
  ~ViewHostMsg_Audio_CreateStream_Params();

  // Parameters for the new audio stream.
  // If |samples_per_packet| is set 0, the audio packet size is selected
  // automatically by the browser process.
  AudioParameters params;
};

// This message is used for supporting popup menus on Mac OS X using native
// Cocoa controls. The renderer sends us this message which we use to populate
// the popup menu.
struct ViewHostMsg_ShowPopup_Params {
  ViewHostMsg_ShowPopup_Params();
  ~ViewHostMsg_ShowPopup_Params();

  // Position on the screen.
  gfx::Rect bounds;

  // The height of each item in the menu.
  int item_height;

  // The size of the font to use for those items.
  double item_font_size;

  // The currently selected (displayed) item in the menu.
  int selected_item;

  // The entire list of items in the popup menu.
  std::vector<WebMenuItem> popup_items;

  // Whether items should be right-aligned.
  bool right_aligned;
};

// Parameters for the IPC message ViewHostMsg_ScriptedPrint
struct ViewHostMsg_ScriptedPrint_Params {
  ViewHostMsg_ScriptedPrint_Params();
  ~ViewHostMsg_ScriptedPrint_Params();

  int routing_id;
  gfx::NativeViewId host_window_id;
  int cookie;
  int expected_pages_count;
  bool has_selection;
  bool use_overlays;
};

// Allows an extension to execute code in a tab.
struct ViewMsg_ExecuteCode_Params {
  ViewMsg_ExecuteCode_Params();
  ViewMsg_ExecuteCode_Params(int request_id, const std::string& extension_id,
                             bool is_javascript, const std::string& code,
                             bool all_frames);
  ~ViewMsg_ExecuteCode_Params();

  // The extension API request id, for responding.
  int request_id;

  // The ID of the requesting extension. To know which isolated world to
  // execute the code inside of.
  std::string extension_id;

  // Whether the code is JavaScript or CSS.
  bool is_javascript;

  // String of code to execute.
  std::string code;

  // Whether to inject into all frames, or only the root frame.
  bool all_frames;
};

// Parameters for the message that creates a worker thread.
struct ViewHostMsg_CreateWorker_Params {
  ViewHostMsg_CreateWorker_Params();
  ~ViewHostMsg_CreateWorker_Params();

  // URL for the worker script.
  GURL url;

  // True if this is a SharedWorker, false if it is a dedicated Worker.
  bool is_shared;

  // Name for a SharedWorker, otherwise empty string.
  string16 name;

  // The ID of the parent document (unique within parent renderer).
  unsigned long long document_id;

  // RenderView routing id used to send messages back to the parent.
  int render_view_route_id;

  // The route ID to associate with the worker. If MSG_ROUTING_NONE is passed,
  // a new unique ID is created and assigned to the worker.
  int route_id;

  // The ID of the parent's appcache host, only valid for dedicated workers.
  int parent_appcache_host_id;

  // The ID of the appcache the main shared worker script resource was loaded
  // from, only valid for shared workers.
  int64 script_resource_appcache_id;
};

// Parameters for the message that creates a desktop notification.
struct ViewHostMsg_ShowNotification_Params {
  ViewHostMsg_ShowNotification_Params();
  ~ViewHostMsg_ShowNotification_Params();

  // URL which is the origin that created this notification.
  GURL origin;

  // True if this is HTML
  bool is_html;

  // URL which contains the HTML contents (if is_html is true), otherwise empty.
  GURL contents_url;

  // Contents of the notification if is_html is false.
  GURL icon_url;
  string16 title;
  string16 body;

  // Directionality of the notification.
  WebKit::WebTextDirection direction;

  // ReplaceID if this notification should replace an existing one; may be
  // empty if no replacement is called for.
  string16 replace_id;

  // Notification ID for sending events back for this notification.
  int notification_id;

  // The routing id of the message.
  int routing_id;
};

// Creates a new view via a control message since the view doesn't yet exist.
struct ViewMsg_New_Params {
  ViewMsg_New_Params();
  ~ViewMsg_New_Params();

  // The parent window's id.
  gfx::NativeViewId parent_window;

  // Renderer-wide preferences.
  RendererPreferences renderer_preferences;

  // Preferences for this view.
  WebPreferences web_preferences;

  // The ID of the view to be created.
  int32 view_id;

  // The session storage namespace ID this view should use.
  int64 session_storage_namespace_id;

  // The name of the frame associated with this view (or empty if none).
  string16 frame_name;
};

struct ViewHostMsg_CreateWindow_Params {
  ViewHostMsg_CreateWindow_Params();
  ~ViewHostMsg_CreateWindow_Params();

  // Routing ID of the view initiating the open.
  int opener_id;

  // True if this open request came in the context of a user gesture.
  bool user_gesture;

  // Type of window requested.
  WindowContainerType window_container_type;

  // The session storage namespace ID this view should use.
  int64 session_storage_namespace_id;

  // The name of the resulting frame that should be created (empty if none
  // has been specified).
  string16 frame_name;

  // The frame identifier of the frame initiating the open.
  int64 opener_frame_id;

  // The URL of the frame initiating the open.
  GURL opener_url;

  // The security origin of the frame initiating the open.
  std::string opener_security_origin;

  // The URL that will be loaded in the new window (empty if none has been
  // sepcified).
  GURL target_url;
};

struct ViewHostMsg_RunFileChooser_Params {
  enum Mode {
    // Requires that the file exists before allowing the user to pick it.
    Open,

    // Like Open, but allows picking multiple files to open.
    OpenMultiple,

    // Like Open, but selects a folder.
    OpenFolder,

    // Allows picking a nonexistent file, and prompts to overwrite if the file
    // already exists.
    Save,
  };

  ViewHostMsg_RunFileChooser_Params();
  ~ViewHostMsg_RunFileChooser_Params();

  Mode mode;

  // Title to be used for the dialog. This may be empty for the default title,
  // which will be either "Open" or "Save" depending on the mode.
  string16 title;

  // Default file name to select in the dialog.
  FilePath default_file_name;

  // A comma-separated MIME types such as "audio/*,text/plain", that is used
  // to restrict selectable files to such types.
  string16 accept_types;
};

struct ViewMsg_ExtensionLoaded_Params {
  ViewMsg_ExtensionLoaded_Params();
  ~ViewMsg_ExtensionLoaded_Params();
  explicit ViewMsg_ExtensionLoaded_Params(const Extension* extension);

  // A copy constructor is needed because this structure can end up getting
  // copied inside the IPC machinery on gcc <= 4.2.
  ViewMsg_ExtensionLoaded_Params(
      const ViewMsg_ExtensionLoaded_Params& other);

  // Creates a new extension from the data in this object.
  scoped_refptr<Extension> ConvertToExtension() const;

  // The subset of the extension manifest data we send to renderers.
  scoped_ptr<DictionaryValue> manifest;

  // The location the extension was installed from.
  Extension::Location location;

  // The path the extension was loaded from. This is used in the renderer only
  // to generate the extension ID for extensions that are loaded unpacked.
  FilePath path;

  // We keep this separate so that it can be used in logging.
  std::string id;
};

struct ViewMsg_DeviceOrientationUpdated_Params {
  ViewMsg_DeviceOrientationUpdated_Params();
  ~ViewMsg_DeviceOrientationUpdated_Params();

  // These fields have the same meaning as in device_orientation::Orientation.
  bool can_provide_alpha;
  double alpha;
  bool can_provide_beta;
  double beta;
  bool can_provide_gamma;
  double gamma;
};

// Parameters structure for ViewHostMsg_ExtensionRequest.
struct ViewHostMsg_DomMessage_Params {
  ViewHostMsg_DomMessage_Params();
  ~ViewHostMsg_DomMessage_Params();

  // Message name.
  std::string name;

  // List of message arguments.
  ListValue arguments;

  // URL of the frame request was sent from.
  GURL source_url;

  // Unique request id to match requests and responses.
  int request_id;

  // True if request has a callback specified.
  bool has_callback;

  // True if request is executed in response to an explicit user gesture.
  bool user_gesture;
};

struct ViewHostMsg_AccessibilityNotification_Params {
  enum NotificationType {
    // The node checked state has changed.
    NOTIFICATION_TYPE_CHECK_STATE_CHANGED,

    // The node tree structure has changed.
    NOTIFICATION_TYPE_CHILDREN_CHANGED,

    // The node in focus has changed.
    NOTIFICATION_TYPE_FOCUS_CHANGED,

    // The document node has loaded.
    NOTIFICATION_TYPE_LOAD_COMPLETE,

    // The node value has changed.
    NOTIFICATION_TYPE_VALUE_CHANGED,

    // The text cursor or selection changed.
    NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED,
  };

  // Type of notification.
  NotificationType notification_type;

  // The accessibility node tree.
  webkit_glue::WebAccessibility acc_obj;
};

namespace IPC {

class Message;

// Traits for ViewMsg_Navigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_Navigate_Params> {
  typedef ViewMsg_Navigate_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_AudioStreamState_Params> {
  typedef ViewMsg_AudioStreamState_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_StopFinding_Params> {
  typedef ViewMsg_StopFinding_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_PageHasOSDD_Type> {
  typedef ViewHostMsg_PageHasOSDD_Type param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params> {
  typedef ViewHostMsg_GetSearchProviderInstallState_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_FrameNavigate_Params> {
  typedef ViewHostMsg_FrameNavigate_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_UpdateRect_Params> {
  typedef ViewHostMsg_UpdateRect_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_ClosePage_Params> {
  typedef ViewMsg_ClosePage_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_Resource_Request> {
  typedef ViewHostMsg_Resource_Request param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_Print_Params> {
  typedef ViewMsg_Print_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_PrintPage_Params> {
  typedef ViewMsg_PrintPage_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_PrintPages_Params> {
  typedef ViewMsg_PrintPages_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_DidPreviewDocument_Params> {
  typedef ViewHostMsg_DidPreviewDocument_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_DidPrintPage_Params> {
  typedef ViewHostMsg_DidPrintPage_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_Audio_CreateStream_Params> {
  typedef ViewHostMsg_Audio_CreateStream_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_ShowPopup_Params> {
  typedef ViewHostMsg_ShowPopup_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_ScriptedPrint_Params> {
  typedef ViewHostMsg_ScriptedPrint_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_ExecuteCode_Params> {
  typedef ViewMsg_ExecuteCode_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_CreateWorker_Params> {
  typedef ViewHostMsg_CreateWorker_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_ShowNotification_Params> {
  typedef ViewHostMsg_ShowNotification_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type &p, std::string* l);
};

template<>
struct ParamTraits<ViewMsg_New_Params> {
  typedef ViewMsg_New_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<ViewHostMsg_CreateWindow_Params> {
  typedef ViewHostMsg_CreateWindow_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<ViewHostMsg_RunFileChooser_Params> {
  typedef ViewHostMsg_RunFileChooser_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_ExtensionLoaded_Params> {
  typedef ViewMsg_ExtensionLoaded_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewMsg_DeviceOrientationUpdated_Params> {
  typedef ViewMsg_DeviceOrientationUpdated_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_DomMessage_Params> {
  typedef ViewHostMsg_DomMessage_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<base::FileUtilProxy::Entry> {
  typedef base::FileUtilProxy::Entry param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_AccessibilityNotification_Params> {
  typedef ViewHostMsg_AccessibilityNotification_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_PARAMS_H_
