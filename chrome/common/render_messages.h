// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

// TODO(erg): This list has been temporarily annotated by erg while doing work
// on which headers to pull out.
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
#include "chrome/common/extensions/extension_extent.h" // used in struct
#include "chrome/common/font_descriptor_mac.h"
#include "chrome/common/indexed_db_param_traits.h"
#include "chrome/common/navigation_gesture.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/renderer_preferences.h"        // used in struct
#include "chrome/common/resource_response.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "chrome/common/webkit_param_traits.h"
#include "chrome/common/window_container_type.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_io.h"
#include "net/base/upload_data.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/WebKit/chromium/public/WebTextDirection.h"
#include "webkit/appcache/appcache_interfaces.h"  // enum appcache::Status
#include "webkit/glue/password_form.h"            // used in struct
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/glue/webpreferences.h"           // used in struct

namespace appcache {
struct AppCacheInfo;
struct AppCacheResourceInfo;
}

namespace base {
class Time;
}

namespace net {
class HttpResponseHeaders;
}

namespace webkit_glue {
struct FormData;
class FormField;
struct PasswordFormFillData;
struct WebAccessibility;
struct WebCookie;
}

namespace webkit_glue {
struct WebAccessibility;
}

struct EditCommand;
class ExtensionExtent;

class SkBitmap;
class URLPattern;
struct ContextMenuParams;
struct WebDropData;
struct WebPluginInfo;
struct WebPluginMimeType;

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

  bool download_to_file;

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
struct ViewHostMsg_IDBFactoryOpen_Params {
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

// Used to open an IndexedDB cursor.
struct ViewHostMsg_IDBObjectStoreOpenCursor_Params {
  // The response should have this id.
  int32 response_id_;
  // The serialized left key.
  IndexedDBKey left_key_;
  // The serialized right key.
  IndexedDBKey right_key_;
  // The key flags.
  int32 flags_;
  // The direction of this cursor.
  int32 direction_;
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

struct ViewMsg_DeviceOrientationUpdated_Params {
  // These fields have the same meaning as in device_orientation::Orientation.
  bool can_provide_alpha;
  double alpha;
  bool can_provide_beta;
  double beta;
  bool can_provide_gamma;
  double gamma;
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

// Traits for ViewHostMsg_UpdateRect_Params structure to pack/unpack.
template <>
struct ParamTraits<ViewHostMsg_UpdateRect_Params> {
  typedef ViewHostMsg_UpdateRect_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<webkit_glue::WebPluginGeometry> {
  typedef webkit_glue::WebPluginGeometry param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

// Traits for ViewMsg_GetPlugins_Reply structure to pack/unpack.
template <>
struct ParamTraits<WebPluginMimeType> {
  typedef WebPluginMimeType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<WebPluginInfo> {
  typedef WebPluginInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

// Traits for webkit_glue::PasswordFormDomManager::FillData.
template <>
struct ParamTraits<webkit_glue::PasswordFormFillData> {
  typedef webkit_glue::PasswordFormFillData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
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
    WriteParam(m, p.download_to_file);
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
      ReadParam(m, iter, &r->download_to_file) &&
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
    LogParam(p.download_to_file, l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
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
    WriteParam(m, p.download_file_path);
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
        ReadParam(m, iter, &r->download_file_path) &&
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
    LogParam(p.download_file_path, l);
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



// Traits for FormData structure to pack/unpack.
template <>
struct ParamTraits<webkit_glue::FormData> {
  typedef webkit_glue::FormData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

// Traits for WebPreferences structure to pack/unpack.
template <>
struct ParamTraits<WebPreferences> {
  typedef WebPreferences param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

// Traits for WebDropData
template <>
struct ParamTraits<WebDropData> {
  typedef WebDropData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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

// Traits for ViewHostMsg_IDBFactoryOpen_Params.
template <>
struct ParamTraits<ViewHostMsg_IDBFactoryOpen_Params> {
  typedef ViewHostMsg_IDBFactoryOpen_Params param_type;
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

// Traits for ViewHostMsg_IDBObjectStoreOpenCursor_Params.
template <>
struct ParamTraits<ViewHostMsg_IDBObjectStoreOpenCursor_Params> {
  typedef ViewHostMsg_IDBObjectStoreOpenCursor_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.response_id_);
    WriteParam(m, p.left_key_);
    WriteParam(m, p.right_key_);
    WriteParam(m, p.flags_);
    WriteParam(m, p.direction_);
    WriteParam(m, p.idb_object_store_id_);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
        ReadParam(m, iter, &p->response_id_) &&
        ReadParam(m, iter, &p->left_key_) &&
        ReadParam(m, iter, &p->right_key_) &&
        ReadParam(m, iter, &p->flags_) &&
        ReadParam(m, iter, &p->direction_) &&
        ReadParam(m, iter, &p->idb_object_store_id_);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.response_id_, l);
    l->append(L", ");
    LogParam(p.left_key_, l);
    l->append(L", ");
    LogParam(p.right_key_, l);
    l->append(L", ");
    LogParam(p.flags_, l);
    l->append(L", ");
    LogParam(p.direction_, l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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

template<>
struct ParamTraits<appcache::AppCacheResourceInfo> {
  typedef appcache::AppCacheResourceInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<appcache::AppCacheInfo> {
  typedef appcache::AppCacheInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
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
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits<webkit_glue::WebAccessibility> {
  typedef webkit_glue::WebAccessibility param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

// Traits for ViewMsg_DeviceOrientationUpdated_Params
// structure to pack/unpack.
template <>
struct ParamTraits<ViewMsg_DeviceOrientationUpdated_Params> {
  typedef ViewMsg_DeviceOrientationUpdated_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.can_provide_alpha);
    WriteParam(m, p.alpha);
    WriteParam(m, p.can_provide_beta);
    WriteParam(m, p.beta);
    WriteParam(m, p.can_provide_gamma);
    WriteParam(m, p.gamma);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->can_provide_alpha) &&
      ReadParam(m, iter, &p->alpha) &&
      ReadParam(m, iter, &p->can_provide_beta) &&
      ReadParam(m, iter, &p->beta) &&
      ReadParam(m, iter, &p->can_provide_gamma) &&
      ReadParam(m, iter, &p->gamma);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.can_provide_alpha, l);
    l->append(L", ");
    LogParam(p.alpha, l);
    l->append(L", ");
    LogParam(p.can_provide_beta, l);
    l->append(L", ");
    LogParam(p.beta, l);
    l->append(L", ");
    LogParam(p.can_provide_gamma, l);
    l->append(L", ");
    LogParam(p.gamma, l);
    l->append(L")");
  }
};
}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/render_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
