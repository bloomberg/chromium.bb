// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

#include <map>
#include <string>
#include <vector>

#include "app/clipboard/clipboard.h"
#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/css_colors.h"
#include "chrome/common/dom_storage_common.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/font_descriptor_mac.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/navigation_gesture.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/resource_response.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/common/webkit_param_traits.h"
#include "chrome/common/window_container_type.h"
#include "gfx/native_widget_types.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_io.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/WebKit/chromium/public/WebTextDirection.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/plugins/webplugininfo.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/glue/webpreferences.h"

namespace base {
class Time;
}

class SkBitmap;

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

    // Navigation type not categorized by the other types.
    NORMAL
  };

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
  GURL referrer;

  // The type of transition.
  PageTransition::Type transition;

  // Opaque history state (received by ViewHostMsg_UpdateState).
  std::string state;

  // Type of navigation.
  NavigationType navigation_type;

  // The time the request was created
  base::Time request_time;
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

  // The action that should be taken when the find is completed.
  Action action;
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
  // Page ID of this navigation. The renderer creates a new unique page ID
  // anytime a new session history entry is created. This means you'll get new
  // page IDs for user actions, and the old page IDs will be reloaded when
  // iframes are loaded automatically.
  int32 page_id;

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

  // The regions of the bitmap (in view coords) that contain updated pixels.
  // In the case of scrolling, this includes the scroll damage rect.
  std::vector<gfx::Rect> copy_rects;

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress.
  gfx::Size view_size;

  // New window locations for plugin child windows.
  std::vector<webkit_glue::WebPluginGeometry> plugin_window_moves;

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

  // URLRequest load flags (0 by default).
  int load_flags;

  // Unique ID of process that originated this request. For normal renderer
  // requests, this will be the ID of the renderer. For plugin requests routed
  // through the renderer, this will be the plugin's ID.
  int origin_child_id;

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  ResourceType::Type resource_type;

  // Used by plugin->browser requests to get the correct URLRequestContext.
  uint32 request_context;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kNoHostId.
  int appcache_host_id;

  // Optional upload data (may be null).
  scoped_refptr<net::UploadData> upload_data;

  // The following two members are specified if the request is initiated by
  // a plugin like Gears.

  // Contains the id of the host renderer.
  int host_renderer_id;

  // Contains the id of the host render view.
  int host_render_view_id;
};

// Parameters for a render request.
struct ViewMsg_Print_Params {
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

  // Warning: do not compare document_cookie.
  bool Equals(const ViewMsg_Print_Params& rhs) const {
    return page_size == rhs.page_size &&
           printable_size == rhs.printable_size &&
           margin_top == rhs.margin_top &&
           margin_left == rhs.margin_left &&
           dpi == rhs.dpi &&
           min_shrink == rhs.min_shrink &&
           max_shrink == rhs.max_shrink &&
           desired_dpi == rhs.desired_dpi &&
           selection_only == rhs.selection_only;
  }

  // Checking if the current params is empty. Just initialized after a memset.
  bool IsEmpty() const {
    return !document_cookie && !desired_dpi && !max_shrink && !min_shrink &&
           !dpi && printable_size.IsEmpty() && !selection_only &&
           page_size.IsEmpty() && !margin_top && !margin_left;
  }
};

struct ViewMsg_PrintPage_Params {
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // The page number is the indicator of the square that should be rendered
  // according to the layout specified in ViewMsg_Print_Params.
  int page_number;
};

struct ViewMsg_PrintPages_Params {
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  ViewMsg_Print_Params params;

  // If empty, this means a request to render all the printed pages.
  std::vector<int> pages;
};

// Parameters to describe a rendered page.
struct ViewHostMsg_DidPrintPage_Params {
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
  // Format request for the stream.
  AudioManager::Format format;

  // Number of channels.
  int channels;

  // Sampling rate (frequency) of the output stream.
  int sample_rate;

  // Number of bits per sample;
  int bits_per_sample;

  // Number of bytes per packet. Determines the maximum number of bytes
  // transported for each audio packet request.
  // A value of 0 means that the audio packet size is selected automatically
  // by the browser process.
  uint32 packet_size;
};

// This message is used for supporting popup menus on Mac OS X using native
// Cocoa controls. The renderer sends us this message which we use to populate
// the popup menu.
struct ViewHostMsg_ShowPopup_Params {
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
  int routing_id;
  gfx::NativeViewId host_window_id;
  int cookie;
  int expected_pages_count;
  bool has_selection;
  bool use_overlays;
};

// Signals a storage event.
struct ViewMsg_DOMStorageEvent_Params {
  // The key that generated the storage event.  Null if clear() was called.
  NullableString16 key_;

  // The old value of this key.  Null on clear() or if it didn't have a value.
  NullableString16 old_value_;

  // The new value of this key.  Null on removeItem() or clear().
  NullableString16 new_value_;

  // The origin this is associated with.
  string16 origin_;

  // The URL of the page that caused the storage event.
  GURL url_;

  // The storage type of this event.
  DOMStorageType storage_type_;
};

// Used to open an indexed database.
struct ViewHostMsg_IndexedDatabaseOpen_Params {
  // The routing ID of the view initiating the open.
  int32 routing_id_;

  // The response should have this id.
  int32 response_id_;

  // The origin doing the initiating.
  string16 origin_;

  // The name of the database.
  string16 name_;

  // The description of the database.
  string16 description_;
};

// Used to create an object store.
struct ViewHostMsg_IDBDatabaseCreateObjectStore_Params {
  // The response should have this id.
  int32 response_id_;

  // The name of the object store.
  string16 name_;

  // The keyPath of the object store.
  NullableString16 key_path_;

  // Whether the object store created should have a key generator.
  bool auto_increment_;

  // The database the object store belongs to.
  int32 idb_database_id_;
};

// Used to create an index.
struct ViewHostMsg_IDBObjectStoreCreateIndex_Params {
  // The response should have this id.
  int32 response_id_;

  // The name of the index.
  string16 name_;

  // The keyPath of the index.
  NullableString16 key_path_;

  // Whether the index created has unique keys.
  bool unique_;

  // The object store the index belongs to.
  int32 idb_object_store_id_;
};

// Allows an extension to execute code in a tab.
struct ViewMsg_ExecuteCode_Params {
  ViewMsg_ExecuteCode_Params() {}
  ViewMsg_ExecuteCode_Params(int request_id, const std::string& extension_id,
                             const std::vector<URLPattern>& host_permissions,
                             bool is_javascript, const std::string& code,
                             bool all_frames)
    : request_id(request_id), extension_id(extension_id),
      host_permissions(host_permissions), is_javascript(is_javascript),
      code(code), all_frames(all_frames) {
  }

  // The extension API request id, for responding.
  int request_id;

  // The ID of the requesting extension. To know which isolated world to
  // execute the code inside of.
  std::string extension_id;

  // The host permissions of the requesting extension. So that we can check them
  // right before injecting, to avoid any race conditions.
  std::vector<URLPattern> host_permissions;

  // Whether the code is JavaScript or CSS.
  bool is_javascript;

  // String of code to execute.
  std::string code;

  // Whether to inject into all frames, or only the root frame.
  bool all_frames;
};

// Parameters for the message that creates a worker thread.
struct ViewHostMsg_CreateWorker_Params {
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
};

// Creates a new view via a control message since the view doesn't yet exist.
struct ViewMsg_New_Params {
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
};

struct ViewHostMsg_RunFileChooser_Params {
  enum Mode {
    // Requires that the file exists before allowing the user to pick it.
    Open,

    // Like Open, but allows picking multiple files to open.
    OpenMultiple,

    // Allows picking a nonexistent file, and prompts to overwrite if the file
    // already exists.
    Save,
  };

  Mode mode;

  // Title to be used for the dialog. This may be empty for the default title,
  // which will be either "Open" or "Save" depending on the mode.
  string16 title;

  // Default file name to select in the dialog.
  FilePath default_file_name;
};

struct ViewMsg_ExtensionExtentInfo {
  std::string extension_id;
  ExtensionExtent web_extent;
  ExtensionExtent browse_extent;
};

struct ViewMsg_ExtensionExtentsUpdated_Params {
  // Describes the installed extension apps and the URLs they cover.
  std::vector<ViewMsg_ExtensionExtentInfo> extension_apps;
};

// Values that may be OR'd together to form the 'flags' parameter of the
// ViewMsg_EnablePreferredSizeChangedMode message.
enum ViewHostMsg_EnablePreferredSizeChangedMode_Flags {
  kPreferredSizeNothing,
  kPreferredSizeWidth = 1 << 0,
  // Requesting the height currently requires a polling loop in render_view.cc.
  kPreferredSizeHeightThisIsSlow = 1 << 1,
};

namespace IPC {

template <>
struct ParamTraits<ResourceType::Type> {
  typedef ResourceType::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type) || !ResourceType::ValidType(type))
      return false;
    *p = ResourceType::FromInt(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case ResourceType::MAIN_FRAME:
        type = L"MAIN_FRAME";
        break;
      case ResourceType::SUB_FRAME:
        type = L"SUB_FRAME";
        break;
      case ResourceType::SUB_RESOURCE:
        type = L"SUB_RESOURCE";
        break;
      case ResourceType::OBJECT:
        type = L"OBJECT";
        break;
      case ResourceType::MEDIA:
        type = L"MEDIA";
        break;
      default:
        type = L"UNKNOWN";
        break;
    }

    LogParam(type, l);
  }
};

// Traits for ViewMsg_Navigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_Navigate_Params> {
  typedef ViewMsg_Navigate_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_id);
    WriteParam(m, p.pending_history_list_offset);
    WriteParam(m, p.current_history_list_offset);
    WriteParam(m, p.current_history_list_length);
    WriteParam(m, p.url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.transition);
    WriteParam(m, p.state);
    WriteParam(m, p.navigation_type);
    WriteParam(m, p.request_time);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->pending_history_list_offset) &&
      ReadParam(m, iter, &p->current_history_list_offset) &&
      ReadParam(m, iter, &p->current_history_list_length) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->state) &&
      ReadParam(m, iter, &p->navigation_type) &&
      ReadParam(m, iter, &p->request_time);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.page_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.transition, l);
    l->append(L", ");
    LogParam(p.state, l);
    l->append(L", ");
    LogParam(p.navigation_type, l);
    l->append(L", ");
    LogParam(p.request_time, l);
    l->append(L")");
  }
};

template<>
struct ParamTraits<ViewMsg_Navigate_Params::NavigationType> {
  typedef ViewMsg_Navigate_Params::NavigationType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<ViewMsg_Navigate_Params::NavigationType>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring event;
    switch (p) {
      case ViewMsg_Navigate_Params::RELOAD:
        event = L"NavigationType_RELOAD";
        break;

    case ViewMsg_Navigate_Params::RELOAD_IGNORING_CACHE:
        event = L"NavigationType_RELOAD_IGNORING_CACHE";
        break;

      case ViewMsg_Navigate_Params::RESTORE:
        event = L"NavigationType_RESTORE";
        break;

      case ViewMsg_Navigate_Params::NORMAL:
        event = L"NavigationType_NORMAL";
        break;

      default:
        event = L"NavigationType_UNKNOWN";
        break;
    }
    LogParam(event, l);
  }
};

// Traits for FormField_Params structure to pack/unpack.
template <>
struct ParamTraits<webkit_glue::FormField> {
  typedef webkit_glue::FormField param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.label());
    WriteParam(m, p.name());
    WriteParam(m, p.value());
    WriteParam(m, p.form_control_type());
    WriteParam(m, p.size());
    WriteParam(m, p.option_strings());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    string16 label, name, value, form_control_type;
    int size = 0;
    std::vector<string16> options;
    bool result = ReadParam(m, iter, &label);
    result = result && ReadParam(m, iter, &name);
    result = result && ReadParam(m, iter, &value);
    result = result && ReadParam(m, iter, &form_control_type);
    result = result && ReadParam(m, iter, &size);
    result = result && ReadParam(m, iter, &options);
    if (!result)
      return false;

    p->set_label(label);
    p->set_name(name);
    p->set_value(value);
    p->set_form_control_type(form_control_type);
    p->set_size(size);
    p->set_option_strings(options);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FormField>");
  }
};

// Traits for FontDescriptor structure to pack/unpack.
template <>
struct ParamTraits<FontDescriptor> {
  typedef FontDescriptor param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.font_name);
    WriteParam(m, p.font_point_size);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return(
        ReadParam(m, iter, &p->font_name) &&
        ReadParam(m, iter, &p->font_point_size));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FontDescriptor>");
  }
};

// Traits for ViewHostMsg_GetSearchProviderInstallState_Params structure to
// pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params> {
  typedef ViewHostMsg_GetSearchProviderInstallState_Params param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.state);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    p->state = static_cast<param_type::State>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring state;
    switch (p.state) {
      case ViewHostMsg_GetSearchProviderInstallState_Params::DENIED:
        state = L"ViewHostMsg_GetSearchProviderInstallState_Params::DENIED";
        break;
      case ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED:
        state =
            L"ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED";
        break;
      case ViewHostMsg_GetSearchProviderInstallState_Params::
          INSTALLED_BUT_NOT_DEFAULT:
        state = L"ViewHostMsg_GetSearchProviderInstallState_Params::"
                L"INSTALLED_BUT_NOT_DEFAULT";
        break;
      case ViewHostMsg_GetSearchProviderInstallState_Params::
          INSTALLED_AS_DEFAULT:
        state = L"ViewHostMsg_GetSearchProviderInstallState_Params::"
                L"INSTALLED_AS_DEFAULT";
        break;
      default:
        state = L"UNKNOWN";
        break;
    }
    LogParam(state, l);
  }
};

// Traits for ViewHostMsg_FrameNavigate_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_FrameNavigate_Params> {
  typedef ViewHostMsg_FrameNavigate_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_id);
    WriteParam(m, p.url);
    WriteParam(m, p.referrer);
    WriteParam(m, p.transition);
    WriteParam(m, p.redirects);
    WriteParam(m, p.should_update_history);
    WriteParam(m, p.searchable_form_url);
    WriteParam(m, p.searchable_form_encoding);
    WriteParam(m, p.password_form);
    WriteParam(m, p.security_info);
    WriteParam(m, p.gesture);
    WriteParam(m, p.contents_mime_type);
    WriteParam(m, p.is_post);
    WriteParam(m, p.is_content_filtered);
    WriteParam(m, p.http_status_code);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->page_id) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->transition) &&
      ReadParam(m, iter, &p->redirects) &&
      ReadParam(m, iter, &p->should_update_history) &&
      ReadParam(m, iter, &p->searchable_form_url) &&
      ReadParam(m, iter, &p->searchable_form_encoding) &&
      ReadParam(m, iter, &p->password_form) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->gesture) &&
      ReadParam(m, iter, &p->contents_mime_type) &&
      ReadParam(m, iter, &p->is_post) &&
      ReadParam(m, iter, &p->is_content_filtered) &&
      ReadParam(m, iter, &p->http_status_code);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.page_id, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.transition, l);
    l->append(L", ");
    LogParam(p.redirects, l);
    l->append(L", ");
    LogParam(p.should_update_history, l);
    l->append(L", ");
    LogParam(p.searchable_form_url, l);
    l->append(L", ");
    LogParam(p.searchable_form_encoding, l);
    l->append(L", ");
    LogParam(p.password_form, l);
    l->append(L", ");
    LogParam(p.security_info, l);
    l->append(L", ");
    LogParam(p.gesture, l);
    l->append(L", ");
    LogParam(p.contents_mime_type, l);
    l->append(L", ");
    LogParam(p.is_post, l);
    l->append(L", ");
    LogParam(p.is_content_filtered, l);
    l->append(L", ");
    LogParam(p.http_status_code, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ContextMenuParams> {
  typedef ContextMenuParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.media_type);
    WriteParam(m, p.x);
    WriteParam(m, p.y);
    WriteParam(m, p.link_url);
    WriteParam(m, p.unfiltered_link_url);
    WriteParam(m, p.src_url);
    WriteParam(m, p.is_image_blocked);
    WriteParam(m, p.page_url);
    WriteParam(m, p.frame_url);
    WriteParam(m, p.media_flags);
    WriteParam(m, p.selection_text);
    WriteParam(m, p.misspelled_word);
    WriteParam(m, p.dictionary_suggestions);
    WriteParam(m, p.spellcheck_enabled);
    WriteParam(m, p.is_editable);
#if defined(OS_MACOSX)
    WriteParam(m, p.writing_direction_default);
    WriteParam(m, p.writing_direction_left_to_right);
    WriteParam(m, p.writing_direction_right_to_left);
#endif  // OS_MACOSX
    WriteParam(m, p.edit_flags);
    WriteParam(m, p.security_info);
    WriteParam(m, p.frame_charset);
    WriteParam(m, p.custom_items);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->media_type) &&
      ReadParam(m, iter, &p->x) &&
      ReadParam(m, iter, &p->y) &&
      ReadParam(m, iter, &p->link_url) &&
      ReadParam(m, iter, &p->unfiltered_link_url) &&
      ReadParam(m, iter, &p->src_url) &&
      ReadParam(m, iter, &p->is_image_blocked) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->frame_url) &&
      ReadParam(m, iter, &p->media_flags) &&
      ReadParam(m, iter, &p->selection_text) &&
      ReadParam(m, iter, &p->misspelled_word) &&
      ReadParam(m, iter, &p->dictionary_suggestions) &&
      ReadParam(m, iter, &p->spellcheck_enabled) &&
      ReadParam(m, iter, &p->is_editable) &&
#if defined(OS_MACOSX)
      ReadParam(m, iter, &p->writing_direction_default) &&
      ReadParam(m, iter, &p->writing_direction_left_to_right) &&
      ReadParam(m, iter, &p->writing_direction_right_to_left) &&
#endif  // OS_MACOSX
      ReadParam(m, iter, &p->edit_flags) &&
      ReadParam(m, iter, &p->security_info) &&
      ReadParam(m, iter, &p->frame_charset) &&
      ReadParam(m, iter, &p->custom_items);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ContextMenuParams>");
  }
};

// Traits for ViewHostMsg_UpdateRect_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_UpdateRect_Params> {
  typedef ViewHostMsg_UpdateRect_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bitmap);
    WriteParam(m, p.bitmap_rect);
    WriteParam(m, p.dx);
    WriteParam(m, p.dy);
    WriteParam(m, p.scroll_rect);
    WriteParam(m, p.copy_rects);
    WriteParam(m, p.view_size);
    WriteParam(m, p.plugin_window_moves);
    WriteParam(m, p.flags);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->bitmap) &&
      ReadParam(m, iter, &p->bitmap_rect) &&
      ReadParam(m, iter, &p->dx) &&
      ReadParam(m, iter, &p->dy) &&
      ReadParam(m, iter, &p->scroll_rect) &&
      ReadParam(m, iter, &p->copy_rects) &&
      ReadParam(m, iter, &p->view_size) &&
      ReadParam(m, iter, &p->plugin_window_moves) &&
      ReadParam(m, iter, &p->flags);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bitmap, l);
    l->append(L", ");
    LogParam(p.bitmap_rect, l);
    l->append(L", ");
    LogParam(p.dx, l);
    l->append(L", ");
    LogParam(p.dy, l);
    l->append(L", ");
    LogParam(p.scroll_rect, l);
    l->append(L", ");
    LogParam(p.copy_rects, l);
    l->append(L", ");
    LogParam(p.view_size, l);
    l->append(L", ");
    LogParam(p.plugin_window_moves, l);
    l->append(L", ");
    LogParam(p.flags, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<webkit_glue::WebPluginGeometry> {
  typedef webkit_glue::WebPluginGeometry param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.window);
    WriteParam(m, p.window_rect);
    WriteParam(m, p.clip_rect);
    WriteParam(m, p.cutout_rects);
    WriteParam(m, p.rects_valid);
    WriteParam(m, p.visible);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->window) &&
      ReadParam(m, iter, &p->window_rect) &&
      ReadParam(m, iter, &p->clip_rect) &&
      ReadParam(m, iter, &p->cutout_rects) &&
      ReadParam(m, iter, &p->rects_valid) &&
      ReadParam(m, iter, &p->visible);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.window, l);
    l->append(L", ");
    LogParam(p.window_rect, l);
    l->append(L", ");
    LogParam(p.clip_rect, l);
    l->append(L", ");
    LogParam(p.cutout_rects, l);
    l->append(L", ");
    LogParam(p.rects_valid, l);
    l->append(L", ");
    LogParam(p.visible, l);
    l->append(L")");
  }
};

// Traits for ViewMsg_GetPlugins_Reply structure to pack/unpack.
template <>
struct ParamTraits<WebPluginMimeType> {
  typedef WebPluginMimeType param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.mime_type);
    WriteParam(m, p.file_extensions);
    WriteParam(m, p.description);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->mime_type) &&
      ReadParam(m, iter, &r->file_extensions) &&
      ReadParam(m, iter, &r->description);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.file_extensions, l);
    l->append(L", ");
    LogParam(p.description, l);
    l->append(L")");
  }
};


template <>
struct ParamTraits<WebPluginInfo> {
  typedef WebPluginInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.name);
    WriteParam(m, p.path);
    WriteParam(m, p.version);
    WriteParam(m, p.desc);
    WriteParam(m, p.mime_types);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->name) &&
      ReadParam(m, iter, &r->path) &&
      ReadParam(m, iter, &r->version) &&
      ReadParam(m, iter, &r->desc) &&
      ReadParam(m, iter, &r->mime_types);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.name, l);
    l->append(L", ");
    l->append(L", ");
    LogParam(p.path, l);
    l->append(L", ");
    LogParam(p.version, l);
    l->append(L", ");
    LogParam(p.desc, l);
    l->append(L", ");
    LogParam(p.mime_types, l);
    l->append(L")");
  }
};

// Traits for webkit_glue::PasswordFormDomManager::FillData.
template <>
struct ParamTraits<webkit_glue::PasswordFormFillData> {
  typedef webkit_glue::PasswordFormFillData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.basic_data);
    WriteParam(m, p.additional_logins);
    WriteParam(m, p.wait_for_username);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->basic_data) &&
      ReadParam(m, iter, &r->additional_logins) &&
      ReadParam(m, iter, &r->wait_for_username);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<PasswordFormFillData>");
  }
};

template<>
struct ParamTraits<NavigationGesture> {
  typedef NavigationGesture param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<NavigationGesture>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring event;
    switch (p) {
      case NavigationGestureUser:
        event = L"GESTURE_USER";
        break;
      case NavigationGestureAuto:
        event = L"GESTURE_AUTO";
        break;
      default:
        event = L"GESTURE_UNKNOWN";
        break;
    }
    LogParam(event, l);
  }
};

// Traits for ViewMsg_Close_Params.
template <>
struct ParamTraits<ViewMsg_ClosePage_Params> {
  typedef ViewMsg_ClosePage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.closing_process_id);
    WriteParam(m, p.closing_route_id);
    WriteParam(m, p.for_cross_site_transition);
    WriteParam(m, p.new_render_process_host_id);
    WriteParam(m, p.new_request_id);
  }

  static bool Read(const Message* m, void** iter, param_type* r) {
    return ReadParam(m, iter, &r->closing_process_id) &&
           ReadParam(m, iter, &r->closing_route_id) &&
           ReadParam(m, iter, &r->for_cross_site_transition) &&
           ReadParam(m, iter, &r->new_render_process_host_id) &&
           ReadParam(m, iter, &r->new_request_id);
  }

  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.closing_process_id, l);
    l->append(L", ");
    LogParam(p.closing_route_id, l);
    l->append(L", ");
    LogParam(p.for_cross_site_transition, l);
    l->append(L", ");
    LogParam(p.new_render_process_host_id, l);
    l->append(L", ");
    LogParam(p.new_request_id, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_Resource_Request
template <>
struct ParamTraits<ViewHostMsg_Resource_Request> {
  typedef ViewHostMsg_Resource_Request param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.method);
    WriteParam(m, p.url);
    WriteParam(m, p.first_party_for_cookies);
    WriteParam(m, p.referrer);
    WriteParam(m, p.frame_origin);
    WriteParam(m, p.main_frame_origin);
    WriteParam(m, p.headers);
    WriteParam(m, p.load_flags);
    WriteParam(m, p.origin_child_id);
    WriteParam(m, p.resource_type);
    WriteParam(m, p.request_context);
    WriteParam(m, p.appcache_host_id);
    WriteParam(m, p.upload_data);
    WriteParam(m, p.host_renderer_id);
    WriteParam(m, p.host_render_view_id);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->method) &&
      ReadParam(m, iter, &r->url) &&
      ReadParam(m, iter, &r->first_party_for_cookies) &&
      ReadParam(m, iter, &r->referrer) &&
      ReadParam(m, iter, &r->frame_origin) &&
      ReadParam(m, iter, &r->main_frame_origin) &&
      ReadParam(m, iter, &r->headers) &&
      ReadParam(m, iter, &r->load_flags) &&
      ReadParam(m, iter, &r->origin_child_id) &&
      ReadParam(m, iter, &r->resource_type) &&
      ReadParam(m, iter, &r->request_context) &&
      ReadParam(m, iter, &r->appcache_host_id) &&
      ReadParam(m, iter, &r->upload_data) &&
      ReadParam(m, iter, &r->host_renderer_id) &&
      ReadParam(m, iter, &r->host_render_view_id);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.method, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.frame_origin, l);
    l->append(L", ");
    LogParam(p.main_frame_origin, l);
    l->append(L", ");
    LogParam(p.load_flags, l);
    l->append(L", ");
    LogParam(p.origin_child_id, l);
    l->append(L", ");
    LogParam(p.resource_type, l);
    l->append(L", ");
    LogParam(p.request_context, l);
    l->append(L", ");
    LogParam(p.appcache_host_id, l);
    l->append(L", ");
    LogParam(p.host_renderer_id, l);
    l->append(L", ");
    LogParam(p.host_render_view_id, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<scoped_refptr<net::HttpResponseHeaders> > {
  typedef scoped_refptr<net::HttpResponseHeaders> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.get() != NULL);
    if (p) {
      // Do not disclose Set-Cookie headers over IPC.
      p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool has_object;
    if (!ReadParam(m, iter, &has_object))
      return false;
    if (has_object)
      *r = new net::HttpResponseHeaders(*m, iter);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<HttpResponseHeaders>");
  }
};

// Traits for webkit_glue::ResourceLoaderBridge::LoadTimingInfo
template <>
struct ParamTraits<webkit_glue::ResourceLoaderBridge::LoadTimingInfo> {
  typedef webkit_glue::ResourceLoaderBridge::LoadTimingInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.base_time.is_null());
    if (p.base_time.is_null())
      return;
    WriteParam(m, p.base_time);
    WriteParam(m, p.proxy_start);
    WriteParam(m, p.proxy_end);
    WriteParam(m, p.dns_start);
    WriteParam(m, p.dns_end);
    WriteParam(m, p.connect_start);
    WriteParam(m, p.connect_end);
    WriteParam(m, p.ssl_start);
    WriteParam(m, p.ssl_end);
    WriteParam(m, p.send_start);
    WriteParam(m, p.send_end);
    WriteParam(m, p.receive_headers_start);
    WriteParam(m, p.receive_headers_end);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool is_null;
    if (!ReadParam(m, iter, &is_null))
      return false;
    if (is_null)
      return true;

    return
        ReadParam(m, iter, &r->base_time) &&
        ReadParam(m, iter, &r->proxy_start) &&
        ReadParam(m, iter, &r->proxy_end) &&
        ReadParam(m, iter, &r->dns_start) &&
        ReadParam(m, iter, &r->dns_end) &&
        ReadParam(m, iter, &r->connect_start) &&
        ReadParam(m, iter, &r->connect_end) &&
        ReadParam(m, iter, &r->ssl_start) &&
        ReadParam(m, iter, &r->ssl_end) &&
        ReadParam(m, iter, &r->send_start) &&
        ReadParam(m, iter, &r->send_end) &&
        ReadParam(m, iter, &r->receive_headers_start) &&
        ReadParam(m, iter, &r->receive_headers_end);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.base_time, l);
    l->append(L", ");
    LogParam(p.proxy_start, l);
    l->append(L", ");
    LogParam(p.proxy_end, l);
    l->append(L", ");
    LogParam(p.dns_start, l);
    l->append(L", ");
    LogParam(p.dns_end, l);
    l->append(L", ");
    LogParam(p.connect_start, l);
    l->append(L", ");
    LogParam(p.connect_end, l);
    l->append(L", ");
    LogParam(p.ssl_start, l);
    l->append(L", ");
    LogParam(p.ssl_end, l);
    l->append(L", ");
    LogParam(p.send_start, l);
    l->append(L", ");
    LogParam(p.send_end, l);
    l->append(L", ");
    LogParam(p.receive_headers_start, l);
    l->append(L", ");
    LogParam(p.receive_headers_end, l);
    l->append(L")");
  }
};

// Traits for webkit_glue::ResourceLoaderBridge::ResponseInfo
template <>
struct ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo> {
  typedef webkit_glue::ResourceLoaderBridge::ResponseInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.request_time);
    WriteParam(m, p.response_time);
    WriteParam(m, p.headers);
    WriteParam(m, p.mime_type);
    WriteParam(m, p.charset);
    WriteParam(m, p.security_info);
    WriteParam(m, p.content_length);
    WriteParam(m, p.appcache_id);
    WriteParam(m, p.appcache_manifest_url);
    WriteParam(m, p.connection_id);
    WriteParam(m, p.connection_reused);
    WriteParam(m, p.load_timing);
    WriteParam(m, p.was_fetched_via_spdy);
    WriteParam(m, p.was_npn_negotiated);
    WriteParam(m, p.was_alternate_protocol_available);
    WriteParam(m, p.was_fetched_via_proxy);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
        ReadParam(m, iter, &r->request_time) &&
        ReadParam(m, iter, &r->response_time) &&
        ReadParam(m, iter, &r->headers) &&
        ReadParam(m, iter, &r->mime_type) &&
        ReadParam(m, iter, &r->charset) &&
        ReadParam(m, iter, &r->security_info) &&
        ReadParam(m, iter, &r->content_length) &&
        ReadParam(m, iter, &r->appcache_id) &&
        ReadParam(m, iter, &r->appcache_manifest_url) &&
        ReadParam(m, iter, &r->connection_id) &&
        ReadParam(m, iter, &r->connection_reused) &&
        ReadParam(m, iter, &r->load_timing) &&
        ReadParam(m, iter, &r->was_fetched_via_spdy) &&
        ReadParam(m, iter, &r->was_npn_negotiated) &&
        ReadParam(m, iter, &r->was_alternate_protocol_available) &&
        ReadParam(m, iter, &r->was_fetched_via_proxy);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.request_time, l);
    l->append(L", ");
    LogParam(p.response_time, l);
    l->append(L", ");
    LogParam(p.headers, l);
    l->append(L", ");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.charset, l);
    l->append(L", ");
    LogParam(p.security_info, l);
    l->append(L", ");
    LogParam(p.content_length, l);
    l->append(L", ");
    LogParam(p.appcache_id, l);
    l->append(L", ");
    LogParam(p.appcache_manifest_url, l);
    l->append(L", ");
    LogParam(p.connection_id, l);
    l->append(L", ");
    LogParam(p.connection_reused, l);
    l->append(L", ");
    LogParam(p.load_timing, l);
    l->append(L", ");
    LogParam(p.was_fetched_via_spdy, l);
    l->append(L", ");
    LogParam(p.was_npn_negotiated, l);
    l->append(L", ");
    LogParam(p.was_alternate_protocol_available, l);
    l->append(L", ");
    LogParam(p.was_fetched_via_proxy, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ResourceResponseHead> {
  typedef ResourceResponseHead param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Write(m, p);
    WriteParam(m, p.status);
    WriteParam(m, p.replace_extension_localization_templates);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Read(
        m, iter, r) &&
        ReadParam(m, iter, &r->status) &&
        ReadParam(m, iter, &r->replace_extension_localization_templates);
  }
  static void Log(const param_type& p, std::wstring* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

template <>
struct ParamTraits<SyncLoadResult> {
  typedef SyncLoadResult param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<ResourceResponseHead>::Write(m, p);
    WriteParam(m, p.final_url);
    WriteParam(m, p.data);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ParamTraits<ResourceResponseHead>::Read(m, iter, r) &&
      ReadParam(m, iter, &r->final_url) &&
      ReadParam(m, iter, &r->data);
  }
  static void Log(const param_type& p, std::wstring* l) {
    // log more?
    ParamTraits<webkit_glue::ResourceLoaderBridge::ResponseInfo>::Log(p, l);
  }
};

template <>
struct ParamTraits<SerializedScriptValue> {
  typedef SerializedScriptValue param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.is_null());
    WriteParam(m, p.is_invalid());
    WriteParam(m, p.data());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool is_null;
    bool is_invalid;
    string16 data;
    bool ok =
      ReadParam(m, iter, &is_null) &&
      ReadParam(m, iter, &is_invalid) &&
      ReadParam(m, iter, &data);
    if (!ok)
      return false;
    r->set_is_null(is_null);
    r->set_is_invalid(is_invalid);
    r->set_data(data);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<SerializedScriptValue>(");
    LogParam(p.is_null(), l);
    l->append(L", ");
    LogParam(p.is_invalid(), l);
    l->append(L", ");
    LogParam(p.data(), l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<IndexedDBKey> {
  typedef IndexedDBKey param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, int(p.type()));
    // TODO(jorlow): Technically, we only need to pack the type being used.
    WriteParam(m, p.string());
    WriteParam(m, p.number());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    string16 string;
    int32 number;
    bool ok =
      ReadParam(m, iter, &type) &&
      ReadParam(m, iter, &string) &&
      ReadParam(m, iter, &number);
    if (!ok)
      return false;
    switch (type) {
      case WebKit::WebIDBKey::NullType:
        r->SetNull();
        return true;
      case WebKit::WebIDBKey::StringType:
        r->Set(string);
        return true;
      case WebKit::WebIDBKey::NumberType:
        r->Set(number);
        return true;
      case WebKit::WebIDBKey::InvalidType:
        r->SetInvalid();
        return true;
    }
    NOTREACHED();
    return false;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<IndexedDBKey>(");
    LogParam(int(p.type()), l);
    l->append(L", ");
    LogParam(p.string(), l);
    l->append(L", ");
    LogParam(p.number(), l);
    l->append(L")");
  }
};

// Traits for FormData structure to pack/unpack.
template <>
struct ParamTraits<webkit_glue::FormData> {
  typedef webkit_glue::FormData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.name);
    WriteParam(m, p.method);
    WriteParam(m, p.origin);
    WriteParam(m, p.action);
    WriteParam(m, p.fields);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->fields);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FormData>");
  }
};

// Traits for ViewMsg_Print_Params
template <>
struct ParamTraits<ViewMsg_Print_Params> {
  typedef ViewMsg_Print_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.page_size);
    WriteParam(m, p.printable_size);
    WriteParam(m, p.margin_top);
    WriteParam(m, p.margin_left);
    WriteParam(m, p.dpi);
    WriteParam(m, p.min_shrink);
    WriteParam(m, p.max_shrink);
    WriteParam(m, p.desired_dpi);
    WriteParam(m, p.document_cookie);
    WriteParam(m, p.selection_only);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->page_size) &&
           ReadParam(m, iter, &p->printable_size) &&
           ReadParam(m, iter, &p->margin_top) &&
           ReadParam(m, iter, &p->margin_left) &&
           ReadParam(m, iter, &p->dpi) &&
           ReadParam(m, iter, &p->min_shrink) &&
           ReadParam(m, iter, &p->max_shrink) &&
           ReadParam(m, iter, &p->desired_dpi) &&
           ReadParam(m, iter, &p->document_cookie) &&
           ReadParam(m, iter, &p->selection_only);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_Print_Params>");
  }
};

// Traits for ViewMsg_PrintPage_Params
template <>
struct ParamTraits<ViewMsg_PrintPage_Params> {
  typedef ViewMsg_PrintPage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.params);
    WriteParam(m, p.page_number);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->params) &&
           ReadParam(m, iter, &p->page_number);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_PrintPage_Params>");
  }
};

// Traits for ViewMsg_PrintPages_Params
template <>
struct ParamTraits<ViewMsg_PrintPages_Params> {
  typedef ViewMsg_PrintPages_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.params);
    WriteParam(m, p.pages);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->params) &&
           ReadParam(m, iter, &p->pages);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_PrintPages_Params>");
  }
};

// Traits for ViewHostMsg_DidPrintPage_Params
template <>
struct ParamTraits<ViewHostMsg_DidPrintPage_Params> {
  typedef ViewHostMsg_DidPrintPage_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.metafile_data_handle);
    WriteParam(m, p.data_size);
    WriteParam(m, p.document_cookie);
    WriteParam(m, p.page_number);
    WriteParam(m, p.actual_shrink);
    WriteParam(m, p.page_size);
    WriteParam(m, p.content_area);
    WriteParam(m, p.has_visible_overlays);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->metafile_data_handle) &&
           ReadParam(m, iter, &p->data_size) &&
           ReadParam(m, iter, &p->document_cookie) &&
           ReadParam(m, iter, &p->page_number) &&
           ReadParam(m, iter, &p->actual_shrink) &&
           ReadParam(m, iter, &p->page_size) &&
           ReadParam(m, iter, &p->content_area) &&
           ReadParam(m, iter, &p->has_visible_overlays);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewHostMsg_DidPrintPage_Params>");
  }
};

// Traits for reading/writing CSS Colors
template <>
struct ParamTraits<CSSColors::CSSColorName> {
  typedef CSSColors::CSSColorName param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, reinterpret_cast<int*>(p));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<CSSColorName>");
  }
};


// Traits for RendererPreferences structure to pack/unpack.
template <>
struct ParamTraits<RendererPreferences> {
  typedef RendererPreferences param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.can_accept_load_drops);
    WriteParam(m, p.should_antialias_text);
    WriteParam(m, static_cast<int>(p.hinting));
    WriteParam(m, static_cast<int>(p.subpixel_rendering));
    WriteParam(m, p.focus_ring_color);
    WriteParam(m, p.thumb_active_color);
    WriteParam(m, p.thumb_inactive_color);
    WriteParam(m, p.track_color);
    WriteParam(m, p.active_selection_bg_color);
    WriteParam(m, p.active_selection_fg_color);
    WriteParam(m, p.inactive_selection_bg_color);
    WriteParam(m, p.inactive_selection_fg_color);
    WriteParam(m, p.browser_handles_top_level_requests);
    WriteParam(m, p.caret_blink_interval);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    if (!ReadParam(m, iter, &p->can_accept_load_drops))
      return false;
    if (!ReadParam(m, iter, &p->should_antialias_text))
      return false;

    int hinting = 0;
    if (!ReadParam(m, iter, &hinting))
      return false;
    p->hinting = static_cast<RendererPreferencesHintingEnum>(hinting);

    int subpixel_rendering = 0;
    if (!ReadParam(m, iter, &subpixel_rendering))
      return false;
    p->subpixel_rendering =
        static_cast<RendererPreferencesSubpixelRenderingEnum>(
            subpixel_rendering);

    int focus_ring_color;
    if (!ReadParam(m, iter, &focus_ring_color))
      return false;
    p->focus_ring_color = focus_ring_color;

    int thumb_active_color, thumb_inactive_color, track_color;
    int active_selection_bg_color, active_selection_fg_color;
    int inactive_selection_bg_color, inactive_selection_fg_color;
    if (!ReadParam(m, iter, &thumb_active_color) ||
        !ReadParam(m, iter, &thumb_inactive_color) ||
        !ReadParam(m, iter, &track_color) ||
        !ReadParam(m, iter, &active_selection_bg_color) ||
        !ReadParam(m, iter, &active_selection_fg_color) ||
        !ReadParam(m, iter, &inactive_selection_bg_color) ||
        !ReadParam(m, iter, &inactive_selection_fg_color))
      return false;
    p->thumb_active_color = thumb_active_color;
    p->thumb_inactive_color = thumb_inactive_color;
    p->track_color = track_color;
    p->active_selection_bg_color = active_selection_bg_color;
    p->active_selection_fg_color = active_selection_fg_color;
    p->inactive_selection_bg_color = inactive_selection_bg_color;
    p->inactive_selection_fg_color = inactive_selection_fg_color;

    if (!ReadParam(m, iter, &p->browser_handles_top_level_requests))
      return false;

    if (!ReadParam(m, iter, &p->caret_blink_interval))
      return false;

    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<RendererPreferences>");
  }
};

// Traits for WebPreferences structure to pack/unpack.
template <>
struct ParamTraits<WebPreferences> {
  typedef WebPreferences param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.standard_font_family);
    WriteParam(m, p.fixed_font_family);
    WriteParam(m, p.serif_font_family);
    WriteParam(m, p.sans_serif_font_family);
    WriteParam(m, p.cursive_font_family);
    WriteParam(m, p.fantasy_font_family);
    WriteParam(m, p.default_font_size);
    WriteParam(m, p.default_fixed_font_size);
    WriteParam(m, p.minimum_font_size);
    WriteParam(m, p.minimum_logical_font_size);
    WriteParam(m, p.default_encoding);
    WriteParam(m, p.javascript_enabled);
    WriteParam(m, p.web_security_enabled);
    WriteParam(m, p.javascript_can_open_windows_automatically);
    WriteParam(m, p.loads_images_automatically);
    WriteParam(m, p.plugins_enabled);
    WriteParam(m, p.dom_paste_enabled);
    WriteParam(m, p.developer_extras_enabled);
    WriteParam(m, p.inspector_settings);
    WriteParam(m, p.site_specific_quirks_enabled);
    WriteParam(m, p.shrinks_standalone_images_to_fit);
    WriteParam(m, p.uses_universal_detector);
    WriteParam(m, p.text_areas_are_resizable);
    WriteParam(m, p.java_enabled);
    WriteParam(m, p.allow_scripts_to_close_windows);
    WriteParam(m, p.uses_page_cache);
    WriteParam(m, p.remote_fonts_enabled);
    WriteParam(m, p.javascript_can_access_clipboard);
    WriteParam(m, p.xss_auditor_enabled);
    WriteParam(m, p.local_storage_enabled);
    WriteParam(m, p.databases_enabled);
    WriteParam(m, p.application_cache_enabled);
    WriteParam(m, p.tabs_to_links);
    WriteParam(m, p.user_style_sheet_enabled);
    WriteParam(m, p.user_style_sheet_location);
    WriteParam(m, p.author_and_user_styles_enabled);
    WriteParam(m, p.allow_universal_access_from_file_urls);
    WriteParam(m, p.allow_file_access_from_file_urls);
    WriteParam(m, p.experimental_webgl_enabled);
    WriteParam(m, p.show_composited_layer_borders);
    WriteParam(m, p.accelerated_compositing_enabled);
    WriteParam(m, p.memory_info_enabled);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->standard_font_family) &&
        ReadParam(m, iter, &p->fixed_font_family) &&
        ReadParam(m, iter, &p->serif_font_family) &&
        ReadParam(m, iter, &p->sans_serif_font_family) &&
        ReadParam(m, iter, &p->cursive_font_family) &&
        ReadParam(m, iter, &p->fantasy_font_family) &&
        ReadParam(m, iter, &p->default_font_size) &&
        ReadParam(m, iter, &p->default_fixed_font_size) &&
        ReadParam(m, iter, &p->minimum_font_size) &&
        ReadParam(m, iter, &p->minimum_logical_font_size) &&
        ReadParam(m, iter, &p->default_encoding) &&
        ReadParam(m, iter, &p->javascript_enabled) &&
        ReadParam(m, iter, &p->web_security_enabled) &&
        ReadParam(m, iter, &p->javascript_can_open_windows_automatically) &&
        ReadParam(m, iter, &p->loads_images_automatically) &&
        ReadParam(m, iter, &p->plugins_enabled) &&
        ReadParam(m, iter, &p->dom_paste_enabled) &&
        ReadParam(m, iter, &p->developer_extras_enabled) &&
        ReadParam(m, iter, &p->inspector_settings) &&
        ReadParam(m, iter, &p->site_specific_quirks_enabled) &&
        ReadParam(m, iter, &p->shrinks_standalone_images_to_fit) &&
        ReadParam(m, iter, &p->uses_universal_detector) &&
        ReadParam(m, iter, &p->text_areas_are_resizable) &&
        ReadParam(m, iter, &p->java_enabled) &&
        ReadParam(m, iter, &p->allow_scripts_to_close_windows) &&
        ReadParam(m, iter, &p->uses_page_cache) &&
        ReadParam(m, iter, &p->remote_fonts_enabled) &&
        ReadParam(m, iter, &p->javascript_can_access_clipboard) &&
        ReadParam(m, iter, &p->xss_auditor_enabled) &&
        ReadParam(m, iter, &p->local_storage_enabled) &&
        ReadParam(m, iter, &p->databases_enabled) &&
        ReadParam(m, iter, &p->application_cache_enabled) &&
        ReadParam(m, iter, &p->tabs_to_links) &&
        ReadParam(m, iter, &p->user_style_sheet_enabled) &&
        ReadParam(m, iter, &p->user_style_sheet_location) &&
        ReadParam(m, iter, &p->author_and_user_styles_enabled) &&
        ReadParam(m, iter, &p->allow_universal_access_from_file_urls) &&
        ReadParam(m, iter, &p->allow_file_access_from_file_urls) &&
        ReadParam(m, iter, &p->experimental_webgl_enabled) &&
        ReadParam(m, iter, &p->show_composited_layer_borders) &&
        ReadParam(m, iter, &p->accelerated_compositing_enabled) &&
        ReadParam(m, iter, &p->memory_info_enabled);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebPreferences>");
  }
};

// Traits for WebDropData
template <>
struct ParamTraits<WebDropData> {
  typedef WebDropData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.identity);
    WriteParam(m, p.url);
    WriteParam(m, p.url_title);
    WriteParam(m, p.download_metadata);
    WriteParam(m, p.file_extension);
    WriteParam(m, p.filenames);
    WriteParam(m, p.plain_text);
    WriteParam(m, p.text_html);
    WriteParam(m, p.html_base_url);
    WriteParam(m, p.file_description_filename);
    WriteParam(m, p.file_contents);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->identity) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->url_title) &&
      ReadParam(m, iter, &p->download_metadata) &&
      ReadParam(m, iter, &p->file_extension) &&
      ReadParam(m, iter, &p->filenames) &&
      ReadParam(m, iter, &p->plain_text) &&
      ReadParam(m, iter, &p->text_html) &&
      ReadParam(m, iter, &p->html_base_url) &&
      ReadParam(m, iter, &p->file_description_filename) &&
      ReadParam(m, iter, &p->file_contents);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebDropData>");
  }
};

// Traits for AudioManager::Format.
template <>
struct ParamTraits<AudioManager::Format> {
  typedef AudioManager::Format param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AudioManager::Format>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring format;
    switch (p) {
     case AudioManager::AUDIO_PCM_LINEAR:
       format = L"AUDIO_PCM_LINEAR";
       break;
     case AudioManager::AUDIO_PCM_LOW_LATENCY:
       format = L"AUDIO_PCM_LOW_LATENCY";
       break;
     case AudioManager::AUDIO_MOCK:
       format = L"AUDIO_MOCK";
       break;
     default:
       format = L"AUDIO_LAST_FORMAT";
       break;
    }
    LogParam(format, l);
  }
};

// Traits for ViewHostMsg_Audio_CreateStream_Params.
template <>
struct ParamTraits<ViewHostMsg_Audio_CreateStream_Params> {
  typedef ViewHostMsg_Audio_CreateStream_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.format);
    WriteParam(m, p.channels);
    WriteParam(m, p.sample_rate);
    WriteParam(m, p.bits_per_sample);
    WriteParam(m, p.packet_size);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->format) &&
      ReadParam(m, iter, &p->channels) &&
      ReadParam(m, iter, &p->sample_rate) &&
      ReadParam(m, iter, &p->bits_per_sample) &&
      ReadParam(m, iter, &p->packet_size);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewHostMsg_Audio_CreateStream_Params>(");
    LogParam(p.format, l);
    l->append(L", ");
    LogParam(p.channels, l);
    l->append(L", ");
    LogParam(p.sample_rate, l);
    l->append(L", ");
    LogParam(p.bits_per_sample, l);
    l->append(L", ");
    LogParam(p.packet_size, l);
    l->append(L")");
  }
};


#if defined(OS_POSIX)

// TODO(port): this shouldn't exist. However, the plugin stuff is really using
// HWNDS (NativeView), and making Windows calls based on them. I've not figured
// out the deal with plugins yet.
template <>
struct ParamTraits<gfx::NativeView> {
  typedef gfx::NativeView param_type;
  static void Write(Message* m, const param_type& p) {
    NOTIMPLEMENTED();
  }

  static bool Read(const Message* m, void** iter, param_type* p) {
    NOTIMPLEMENTED();
    *p = NULL;
    return true;
  }

  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"<gfx::NativeView>"));
  }
};

#endif  // defined(OS_POSIX)

template <>
struct ParamTraits<ViewMsg_AudioStreamState_Params> {
  typedef ViewMsg_AudioStreamState_Params param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.state);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    p->state = static_cast<ViewMsg_AudioStreamState_Params::State>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring state;
    switch (p.state) {
     case ViewMsg_AudioStreamState_Params::kPlaying:
       state = L"ViewMsg_AudioStreamState_Params::kPlaying";
       break;
     case ViewMsg_AudioStreamState_Params::kPaused:
       state = L"ViewMsg_AudioStreamState_Params::kPaused";
       break;
     case ViewMsg_AudioStreamState_Params::kError:
       state = L"ViewMsg_AudioStreamState_Params::kError";
       break;
     default:
       state = L"UNKNOWN";
       break;
    }
    LogParam(state, l);
  }
};

template <>
struct ParamTraits<ViewMsg_StopFinding_Params> {
  typedef ViewMsg_StopFinding_Params param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.action);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    p->action = static_cast<ViewMsg_StopFinding_Params::Action>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring action;
    switch (p.action) {
     case ViewMsg_StopFinding_Params::kClearSelection:
       action = L"ViewMsg_StopFinding_Params::kClearSelection";
       break;
     case ViewMsg_StopFinding_Params::kKeepSelection:
       action = L"ViewMsg_StopFinding_Params::kKeepSelection";
       break;
     case ViewMsg_StopFinding_Params::kActivateSelection:
       action = L"ViewMsg_StopFinding_Params::kActivateSelection";
       break;
     default:
       action = L"UNKNOWN";
       break;
    }
    LogParam(action, l);
  }
};

template <>
struct ParamTraits<appcache::Status> {
  typedef appcache::Status param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring state;
    switch (p) {
      case appcache::UNCACHED:
        state = L"UNCACHED";
        break;
      case appcache::IDLE:
        state = L"IDLE";
        break;
      case appcache::CHECKING:
        state = L"CHECKING";
        break;
      case appcache::DOWNLOADING:
        state = L"DOWNLOADING";
        break;
      case appcache::UPDATE_READY:
        state = L"UPDATE_READY";
        break;
      case appcache::OBSOLETE:
        state = L"OBSOLETE";
        break;
      default:
        state = L"InvalidStatusValue";
        break;
    }

    LogParam(state, l);
  }
};

template <>
struct ParamTraits<appcache::EventID> {
  typedef appcache::EventID param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring state;
    switch (p) {
      case appcache::CHECKING_EVENT:
        state = L"CHECKING_EVENT";
        break;
      case appcache::ERROR_EVENT:
        state = L"ERROR_EVENT";
        break;
      case appcache::NO_UPDATE_EVENT:
        state = L"NO_UPDATE_EVENT";
        break;
      case appcache::DOWNLOADING_EVENT:
        state = L"DOWNLOADING_EVENT";
        break;
      case appcache::PROGRESS_EVENT:
        state = L"PROGRESS_EVENT";
        break;
      case appcache::UPDATE_READY_EVENT:
        state = L"UPDATE_READY_EVENT";
        break;
      case appcache::CACHED_EVENT:
        state = L"CACHED_EVENT";
        break;
      case appcache::OBSOLETE_EVENT:
        state = L"OBSOLETE_EVENT";
        break;
      default:
        state = L"InvalidEventValue";
        break;
    }

    LogParam(state, l);
  }
};

template<>
struct ParamTraits<WebMenuItem::Type> {
  typedef WebMenuItem::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<WebMenuItem::Type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case WebMenuItem::OPTION:
        type = L"OPTION";
        break;
      case WebMenuItem::GROUP:
        type = L"GROUP";
        break;
      case WebMenuItem::SEPARATOR:
        type = L"SEPARATOR";
        break;
      default:
        type = L"UNKNOWN";
        break;
    }
    LogParam(type, l);
  }
};

template<>
struct ParamTraits<WebMenuItem> {
  typedef WebMenuItem param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.label);
    WriteParam(m, p.type);
    WriteParam(m, p.enabled);
    WriteParam(m, p.checked);
    WriteParam(m, p.action);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->label) &&
        ReadParam(m, iter, &p->type) &&
        ReadParam(m, iter, &p->enabled) &&
        ReadParam(m, iter, &p->checked) &&
        ReadParam(m, iter, &p->action);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.label, l);
    l->append(L", ");
    LogParam(p.type, l);
    l->append(L", ");
    LogParam(p.enabled, l);
    l->append(L", ");
    LogParam(p.checked, l);
    l->append(L", ");
    LogParam(p.action, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_ShowPopup_Params.
template <>
struct ParamTraits<ViewHostMsg_ShowPopup_Params> {
  typedef ViewHostMsg_ShowPopup_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.bounds);
    WriteParam(m, p.item_height);
    WriteParam(m, p.item_font_size);
    WriteParam(m, p.selected_item);
    WriteParam(m, p.popup_items);
    WriteParam(m, p.right_aligned);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->bounds) &&
        ReadParam(m, iter, &p->item_height) &&
        ReadParam(m, iter, &p->item_font_size) &&
        ReadParam(m, iter, &p->selected_item) &&
        ReadParam(m, iter, &p->popup_items) &&
        ReadParam(m, iter, &p->right_aligned);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.bounds, l);
    l->append(L", ");
    LogParam(p.item_height, l);
    l->append(L", ");
    LogParam(p.item_font_size, l);
    l->append(L", ");
    LogParam(p.selected_item, l);
    l->append(L", ");
    LogParam(p.popup_items, l);
    l->append(L", ");
    LogParam(p.right_aligned, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_ScriptedPrint_Params.
template <>
struct ParamTraits<ViewHostMsg_ScriptedPrint_Params> {
  typedef ViewHostMsg_ScriptedPrint_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.routing_id);
    WriteParam(m, p.host_window_id);
    WriteParam(m, p.cookie);
    WriteParam(m, p.expected_pages_count);
    WriteParam(m, p.has_selection);
    WriteParam(m, p.use_overlays);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->routing_id) &&
        ReadParam(m, iter, &p->host_window_id) &&
        ReadParam(m, iter, &p->cookie) &&
        ReadParam(m, iter, &p->expected_pages_count) &&
        ReadParam(m, iter, &p->has_selection) &&
        ReadParam(m, iter, &p->use_overlays);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.routing_id, l);
    l->append(L", ");
    LogParam(p.host_window_id, l);
    l->append(L", ");
    LogParam(p.cookie, l);
    l->append(L", ");
    LogParam(p.expected_pages_count, l);
    l->append(L", ");
    LogParam(p.has_selection, l);
    l->append(L",");
    LogParam(p.use_overlays, l);
    l->append(L")");
  }
};

template <>
struct SimilarTypeTraits<ViewType::Type> {
  typedef int Type;
};

// Traits for URLPattern.
template <>
struct ParamTraits<URLPattern> {
  typedef URLPattern param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.valid_schemes());
    WriteParam(m, p.GetAsString());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int valid_schemes;
    std::string spec;
    if (!ReadParam(m, iter, &valid_schemes) ||
        !ReadParam(m, iter, &spec))
      return false;

    p->set_valid_schemes(valid_schemes);
    return p->Parse(spec);
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.GetAsString(), l);
  }
};

template <>
struct ParamTraits<Clipboard::Buffer> {
  typedef Clipboard::Buffer param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int buffer;
    if (!m->ReadInt(iter, &buffer) || !Clipboard::IsValidBuffer(buffer))
      return false;
    *p = Clipboard::FromInt(buffer);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
      case Clipboard::BUFFER_STANDARD:
        type = L"BUFFER_STANDARD";
        break;
#if defined(USE_X11)
      case Clipboard::BUFFER_SELECTION:
        type = L"BUFFER_SELECTION";
        break;
#endif
      case Clipboard::BUFFER_DRAG:
        type = L"BUFFER_DRAG";
        break;
      default:
        type = L"UNKNOWN";
        break;
    }

    LogParam(type, l);
  }
};

// Traits for EditCommand structure.
template <>
struct ParamTraits<EditCommand> {
  typedef EditCommand param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.name);
    WriteParam(m, p.value);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->name) && ReadParam(m, iter, &p->value);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.name, l);
    l->append(L":");
    LogParam(p.value, l);
    l->append(L")");
  }
};

// Traits for DOMStorageType enum.
template <>
struct ParamTraits<DOMStorageType> {
  typedef DOMStorageType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
      case DOM_STORAGE_LOCAL:
        control = L"DOM_STORAGE_LOCAL";
        break;
      case DOM_STORAGE_SESSION:
        control = L"DOM_STORAGE_SESSION";
        break;
      default:
        NOTIMPLEMENTED();
        control = L"UNKNOWN";
        break;
    }
    LogParam(control, l);
  }
};

// Traits for WebKit::WebStorageArea::Result enum.
template <>
struct ParamTraits<WebKit::WebStorageArea::Result> {
  typedef WebKit::WebStorageArea::Result param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<param_type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
      case WebKit::WebStorageArea::ResultOK:
        control = L"WebKit::WebStorageArea::ResultOK";
        break;
      case WebKit::WebStorageArea::ResultBlockedByQuota:
        control = L"WebKit::WebStorageArea::ResultBlockedByQuota";
        break;
      case WebKit::WebStorageArea::ResultBlockedByPolicy:
        control = L"WebKit::WebStorageArea::ResultBlockedByPolicy";
        break;
      default:
        NOTIMPLEMENTED();
        control = L"UNKNOWN";
        break;
    }
    LogParam(control, l);
  }
};

// Traits for ViewMsg_DOMStorageEvent_Params.
template <>
struct ParamTraits<ViewMsg_DOMStorageEvent_Params> {
  typedef ViewMsg_DOMStorageEvent_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.key_);
    WriteParam(m, p.old_value_);
    WriteParam(m, p.new_value_);
    WriteParam(m, p.origin_);
    WriteParam(m, p.url_);
    WriteParam(m, p.storage_type_);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->key_) &&
        ReadParam(m, iter, &p->old_value_) &&
        ReadParam(m, iter, &p->new_value_) &&
        ReadParam(m, iter, &p->origin_) &&
        ReadParam(m, iter, &p->url_) &&
        ReadParam(m, iter, &p->storage_type_);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.key_, l);
    l->append(L", ");
    LogParam(p.old_value_, l);
    l->append(L", ");
    LogParam(p.new_value_, l);
    l->append(L", ");
    LogParam(p.origin_, l);
    l->append(L", ");
    LogParam(p.url_, l);
    l->append(L", ");
    LogParam(p.storage_type_, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_IndexedDatabaseOpen_Params.
template <>
struct ParamTraits<ViewHostMsg_IndexedDatabaseOpen_Params> {
  typedef ViewHostMsg_IndexedDatabaseOpen_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.routing_id_);
    WriteParam(m, p.response_id_);
    WriteParam(m, p.origin_);
    WriteParam(m, p.name_);
    WriteParam(m, p.description_);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->routing_id_) &&
        ReadParam(m, iter, &p->response_id_) &&
        ReadParam(m, iter, &p->origin_) &&
        ReadParam(m, iter, &p->name_) &&
        ReadParam(m, iter, &p->description_);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.routing_id_, l);
    l->append(L", ");
    LogParam(p.response_id_, l);
    l->append(L", ");
    LogParam(p.origin_, l);
    l->append(L", ");
    LogParam(p.name_, l);
    l->append(L", ");
    LogParam(p.description_, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_IDBDatabaseCreateObjectStore_Params.
template <>
struct ParamTraits<ViewHostMsg_IDBDatabaseCreateObjectStore_Params> {
  typedef ViewHostMsg_IDBDatabaseCreateObjectStore_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.response_id_);
    WriteParam(m, p.name_);
    WriteParam(m, p.key_path_);
    WriteParam(m, p.auto_increment_);
    WriteParam(m, p.idb_database_id_);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->response_id_) &&
        ReadParam(m, iter, &p->name_) &&
        ReadParam(m, iter, &p->key_path_) &&
        ReadParam(m, iter, &p->auto_increment_) &&
        ReadParam(m, iter, &p->idb_database_id_);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.response_id_, l);
    l->append(L", ");
    LogParam(p.name_, l);
    l->append(L", ");
    LogParam(p.key_path_, l);
    l->append(L", ");
    LogParam(p.auto_increment_, l);
    l->append(L", ");
    LogParam(p.idb_database_id_, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_IDBObjectStoreCreateIndex_Params.
template <>
struct ParamTraits<ViewHostMsg_IDBObjectStoreCreateIndex_Params> {
  typedef ViewHostMsg_IDBObjectStoreCreateIndex_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.response_id_);
    WriteParam(m, p.name_);
    WriteParam(m, p.key_path_);
    WriteParam(m, p.unique_);
    WriteParam(m, p.idb_object_store_id_);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->response_id_) &&
        ReadParam(m, iter, &p->name_) &&
        ReadParam(m, iter, &p->key_path_) &&
        ReadParam(m, iter, &p->unique_) &&
        ReadParam(m, iter, &p->idb_object_store_id_);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.response_id_, l);
    l->append(L", ");
    LogParam(p.name_, l);
    l->append(L", ");
    LogParam(p.key_path_, l);
    l->append(L", ");
    LogParam(p.unique_, l);
    l->append(L", ");
    LogParam(p.idb_object_store_id_, l);
    l->append(L")");
  }
};

// Traits for ViewHostMsg_CreateWorker_Params
template <>
struct ParamTraits<ViewHostMsg_CreateWorker_Params> {
  typedef ViewHostMsg_CreateWorker_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.is_shared);
    WriteParam(m, p.name);
    WriteParam(m, p.document_id);
    WriteParam(m, p.render_view_route_id);
    WriteParam(m, p.route_id);
    WriteParam(m, p.parent_appcache_host_id);
    WriteParam(m, p.script_resource_appcache_id);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->url) &&
        ReadParam(m, iter, &p->is_shared) &&
        ReadParam(m, iter, &p->name) &&
        ReadParam(m, iter, &p->document_id) &&
        ReadParam(m, iter, &p->render_view_route_id) &&
        ReadParam(m, iter, &p->route_id) &&
        ReadParam(m, iter, &p->parent_appcache_host_id) &&
        ReadParam(m, iter, &p->script_resource_appcache_id);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.is_shared, l);
    l->append(L", ");
    LogParam(p.name, l);
    l->append(L", ");
    LogParam(p.document_id, l);
    l->append(L", ");
    LogParam(p.render_view_route_id, l);
    l->append(L",");
    LogParam(p.route_id, l);
    l->append(L", ");
    LogParam(p.parent_appcache_host_id, l);
    l->append(L",");
    LogParam(p.script_resource_appcache_id, l);
    l->append(L")");
  }
};

// Traits for ShowNotification_Params
template <>
struct ParamTraits<ViewHostMsg_ShowNotification_Params> {
  typedef ViewHostMsg_ShowNotification_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.origin);
    WriteParam(m, p.is_html);
    WriteParam(m, p.contents_url);
    WriteParam(m, p.icon_url);
    WriteParam(m, p.title);
    WriteParam(m, p.body);
    WriteParam(m, p.direction);
    WriteParam(m, p.replace_id);
    WriteParam(m, p.notification_id);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->origin) &&
        ReadParam(m, iter, &p->is_html) &&
        ReadParam(m, iter, &p->contents_url) &&
        ReadParam(m, iter, &p->icon_url) &&
        ReadParam(m, iter, &p->title) &&
        ReadParam(m, iter, &p->body) &&
        ReadParam(m, iter, &p->direction) &&
        ReadParam(m, iter, &p->replace_id) &&
        ReadParam(m, iter, &p->notification_id);
  }
  static void Log(const param_type &p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.origin, l);
    l->append(L", ");
    LogParam(p.is_html, l);
    l->append(L", ");
    LogParam(p.contents_url, l);
    l->append(L", ");
    LogParam(p.icon_url, l);
    l->append(L", ");
    LogParam(p.title, l);
    l->append(L",");
    LogParam(p.body, l);
    l->append(L",");
    LogParam(p.direction, l);
    l->append(L",");
    LogParam(p.replace_id, l);
    l->append(L",");
    LogParam(p.notification_id, l);
    l->append(L")");
  }
};

// Traits for WebCookie
template <>
struct ParamTraits<webkit_glue::WebCookie> {
  typedef webkit_glue::WebCookie param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.name);
    WriteParam(m, p.value);
    WriteParam(m, p.domain);
    WriteParam(m, p.path);
    WriteParam(m, p.expires);
    WriteParam(m, p.http_only);
    WriteParam(m, p.secure);
    WriteParam(m, p.session);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->value) &&
      ReadParam(m, iter, &p->domain) &&
      ReadParam(m, iter, &p->path) &&
      ReadParam(m, iter, &p->expires) &&
      ReadParam(m, iter, &p->http_only) &&
      ReadParam(m, iter, &p->secure) &&
      ReadParam(m, iter, &p->session);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebCookie>");
  }
};

template<>
struct ParamTraits<ViewMsg_ExecuteCode_Params> {
  typedef ViewMsg_ExecuteCode_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.request_id);
    WriteParam(m, p.extension_id);
    WriteParam(m, p.host_permissions);
    WriteParam(m, p.is_javascript);
    WriteParam(m, p.code);
    WriteParam(m, p.all_frames);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->extension_id) &&
      ReadParam(m, iter, &p->host_permissions) &&
      ReadParam(m, iter, &p->is_javascript) &&
      ReadParam(m, iter, &p->code) &&
      ReadParam(m, iter, &p->all_frames);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<ViewMsg_ExecuteCode_Params>");
  }
};

template<>
struct ParamTraits<ViewMsg_New_Params> {
  typedef ViewMsg_New_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.parent_window);
    WriteParam(m, p.renderer_preferences);
    WriteParam(m, p.web_preferences);
    WriteParam(m, p.view_id);
    WriteParam(m, p.session_storage_namespace_id);
    WriteParam(m, p.frame_name);
  }

  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->parent_window) &&
      ReadParam(m, iter, &p->renderer_preferences) &&
      ReadParam(m, iter, &p->web_preferences) &&
      ReadParam(m, iter, &p->view_id) &&
      ReadParam(m, iter, &p->session_storage_namespace_id) &&
      ReadParam(m, iter, &p->frame_name);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.parent_window, l);
    l->append(L", ");
    LogParam(p.renderer_preferences, l);
    l->append(L", ");
    LogParam(p.web_preferences, l);
    l->append(L", ");
    LogParam(p.view_id, l);
    l->append(L", ");
    LogParam(p.session_storage_namespace_id, l);
    l->append(L", ");
    LogParam(p.frame_name, l);
    l->append(L")");
  }
};

template <>
struct SimilarTypeTraits<TranslateErrors::Type> {
  typedef int Type;
};

template<>
struct ParamTraits<ViewHostMsg_RunFileChooser_Params> {
  typedef ViewHostMsg_RunFileChooser_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.mode));
    WriteParam(m, p.title);
    WriteParam(m, p.default_file_name);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int mode;
    if (!ReadParam(m, iter, &mode))
      return false;
    if (mode != param_type::Open &&
        mode != param_type::OpenMultiple &&
        mode != param_type::Save)
      return false;
    p->mode = static_cast<param_type::Mode>(mode);
    return
      ReadParam(m, iter, &p->title) &&
      ReadParam(m, iter, &p->default_file_name);
  };
  static void Log(const param_type& p, std::wstring* l) {
    switch (p.mode) {
      case param_type::Open:
        l->append(L"(Open, ");
        break;
      case param_type::OpenMultiple:
        l->append(L"(OpenMultiple, ");
        break;
      case param_type::Save:
        l->append(L"(Save, ");
        break;
      default:
        l->append(L"(UNKNOWN, ");
    }
    LogParam(p.title, l);
    l->append(L", ");
    LogParam(p.default_file_name, l);
  }
};

template<>
struct ParamTraits<ViewHostMsg_CreateWindow_Params> {
  typedef ViewHostMsg_CreateWindow_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.opener_id);
    WriteParam(m, p.user_gesture);
    WriteParam(m, p.window_container_type);
    WriteParam(m, p.session_storage_namespace_id);
    WriteParam(m, p.frame_name);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->opener_id) &&
      ReadParam(m, iter, &p->user_gesture) &&
      ReadParam(m, iter, &p->window_container_type) &&
      ReadParam(m, iter, &p->session_storage_namespace_id) &&
      ReadParam(m, iter, &p->frame_name);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.opener_id, l);
    l->append(L", ");
    LogParam(p.user_gesture, l);
    l->append(L", ");
    LogParam(p.window_container_type, l);
    l->append(L", ");
    LogParam(p.session_storage_namespace_id, l);
    l->append(L", ");
    LogParam(p.frame_name, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<ExtensionExtent> {
  typedef ExtensionExtent param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.patterns());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    std::vector<URLPattern> patterns;
    bool success =
        ReadParam(m, iter, &patterns);
    if (!success)
      return false;

    for (size_t i = 0; i < patterns.size(); ++i)
      p->AddPattern(patterns[i]);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.patterns(), l);
  }
};

template <>
struct ParamTraits<ViewMsg_ExtensionExtentInfo> {
  typedef ViewMsg_ExtensionExtentInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.extension_id);
    WriteParam(m, p.web_extent);
    WriteParam(m, p.browse_extent);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->extension_id) &&
        ReadParam(m, iter, &p->web_extent) &&
        ReadParam(m, iter, &p->browse_extent);
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.extension_id, l);
  }
};

template <>
struct ParamTraits<ViewMsg_ExtensionExtentsUpdated_Params> {
  typedef ViewMsg_ExtensionExtentsUpdated_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.extension_apps);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->extension_apps);
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.extension_apps, l);
  }
};

template <>
struct ParamTraits<WindowContainerType> {
  typedef WindowContainerType param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < WINDOW_CONTAINER_TYPE_NORMAL ||
        val >= WINDOW_CONTAINER_TYPE_MAX_VALUE)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p, l);
  }
};

template <>
struct ParamTraits<webkit_glue::WebAccessibility> {
  typedef webkit_glue::WebAccessibility param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.id);
    WriteParam(m, p.name);
    WriteParam(m, p.value);
    WriteParam(m, static_cast<int>(p.role));
    WriteParam(m, static_cast<int>(p.state));
    WriteParam(m, p.location);
    WriteParam(m, p.attributes);
    WriteParam(m, p.children);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    bool ret = ReadParam(m, iter, &p->id);
    ret = ret && ReadParam(m, iter, &p->name);
    ret = ret && ReadParam(m, iter, &p->value);
    int role = -1;
    ret = ret && ReadParam(m, iter, &role);
    if (role >= webkit_glue::WebAccessibility::ROLE_NONE &&
      role < webkit_glue::WebAccessibility::NUM_ROLES) {
      p->role = static_cast<webkit_glue::WebAccessibility::Role>(role);
    } else {
      p->role = webkit_glue::WebAccessibility::ROLE_NONE;
    }
    int state = 0;
    ret = ret && ReadParam(m, iter, &state);
    p->state = static_cast<webkit_glue::WebAccessibility::State>(state);
    ret = ret && ReadParam(m, iter, &p->location);
    ret = ret && ReadParam(m, iter, &p->attributes);
    ret = ret && ReadParam(m, iter, &p->children);
    return ret;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.id, l);
    l->append(L", ");
    LogParam(p.name, l);
    l->append(L", ");
    LogParam(p.value, l);
    l->append(L", ");
    LogParam(static_cast<int>(p.role), l);
    l->append(L", ");
    LogParam(static_cast<int>(p.state), l);
    l->append(L", ");
    LogParam(p.location, l);
    l->append(L", ");
    LogParam(p.attributes, l);
    l->append(L", ");
    LogParam(p.children, l);
    l->append(L")");
  }
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/render_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
