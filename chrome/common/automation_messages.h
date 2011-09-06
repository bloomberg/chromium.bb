// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/content_settings.h"
#include "content/common/common_param_traits.h"
#include "content/common/page_type.h"
#include "content/common/security_style.h"
#include "content/common/webkit_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data.h"
#include "ui/gfx/rect.h"

// Singly-included section, not yet converted.
#ifndef CHROME_COMMON_AUTOMATION_MESSAGES_H__
#define CHROME_COMMON_AUTOMATION_MESSAGES_H__

struct AutomationMsg_Find_Params {
  // Unused value, which exists only for backwards compat.
  int unused;

  // The word(s) to find on the page.
  string16 search_string;

  // Whether to search forward or backward within the page.
  bool forward;

  // Whether search should be Case sensitive.
  bool match_case;

  // Whether this operation is first request (Find) or a follow-up (FindNext).
  bool find_next;
};

struct AutomationURLResponse {
  AutomationURLResponse();
  AutomationURLResponse(const std::string& mime_type,
                        const std::string& headers,
                        int64 content_length,
                        const base::Time& last_modified,
                        const std::string& redirect_url,
                        int redirect_status,
                        const net::HostPortPair& host_socket_address);
  ~AutomationURLResponse();

  std::string mime_type;
  std::string headers;
  int64 content_length;
  base::Time last_modified;
  std::string redirect_url;
  int redirect_status;
  net::HostPortPair socket_address;
};

struct ExternalTabSettings {
  ExternalTabSettings();
  ExternalTabSettings(gfx::NativeWindow parent,
                      const gfx::Rect& dimensions,
                      unsigned int style,
                      bool is_incognito,
                      bool load_requests_via_automation,
                      bool handle_top_level_requests,
                      const GURL& initial_url,
                      const GURL& referrer,
                      bool infobars_enabled,
                      bool route_all_top_level_navigations);
  ~ExternalTabSettings();

  gfx::NativeWindow parent;
  gfx::Rect dimensions;
  unsigned int style;
  bool is_incognito;
  bool load_requests_via_automation;
  bool handle_top_level_requests;
  GURL initial_url;
  GURL referrer;
  bool infobars_enabled;
  bool route_all_top_level_navigations;
};

struct NavigationInfo {
  NavigationInfo();
  NavigationInfo(int navigation_type,
                 int relative_offset,
                 int navigation_index,
                 const std::wstring& title,
                 const GURL& url,
                 const GURL& referrer,
                 SecurityStyle security_style,
                 bool displayed_insecure_content,
                 bool ran_insecure_content);
  ~NavigationInfo();

  int navigation_type;
  int relative_offset;
  int navigation_index;
  std::wstring title;
  GURL url;
  GURL referrer;
  SecurityStyle security_style;
  bool displayed_insecure_content;
  bool ran_insecure_content;
};

// A stripped down version of ContextMenuParams in webkit/glue/context_menu.h.
struct MiniContextMenuParams {
  MiniContextMenuParams();
  MiniContextMenuParams(int screen_x,
                        int screen_y,
                        const GURL& link_url,
                        const GURL& unfiltered_link_url,
                        const GURL& src_url,
                        const GURL& page_url,
                        const GURL& keyword_url,
                        const GURL& frame_url);
  ~MiniContextMenuParams();

  // The x coordinate for displaying the menu.
  int screen_x;

  // The y coordinate for displaying the menu.
  int screen_y;

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  GURL link_url;

  // The link URL to be used ONLY for "copy link address". We don't validate
  // this field in the frontend process.
  GURL unfiltered_link_url;

  // This is the source URL for the element that the context menu was
  // invoked on.  Example of elements with source URLs are img, audio, and
  // video.
  GURL src_url;

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the absolute keyword search URL including the %s search tag when
  // the "Add as search engine..." option is clicked (left empty if not used).
  GURL keyword_url;

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;
};

// This struct passes information about the context menu in Chrome stored
// as a ui::MenuModel to Chrome Frame.  It is basically a container of
// items that go in the menu.  An item may be a submenu, so the data
// structure may be a tree.
struct ContextMenuModel {
  ContextMenuModel();
  ~ContextMenuModel();

  // This struct describes one item in the menu.
  struct Item {
    Item();

    // This is the type of the menu item, using values from the enum
    // ui::MenuModel::ItemType (serialized as an int).
    int type;

    // This is the command id of the menu item, which will be passed by
    // Chrome Frame to Chrome if the item is selected.
    int item_id;

    // This the the menu label, if needed.
    std::wstring label;

    // These are states of the menu item.
    bool checked;
    bool enabled;

    // This recursively describes the submenu if type is
    // ui::MenuModel::TYPE_SUBMENU.
    ContextMenuModel* submenu;
  };

  // This is the list of menu items.
  std::vector<Item> items;
};

struct AttachExternalTabParams {
  AttachExternalTabParams();
  AttachExternalTabParams(uint64 cookie,
                          const GURL& url,
                          const gfx::Rect& dimensions,
                          int disposition,
                          bool user_gesture,
                          const std::string& profile_name);
  ~AttachExternalTabParams();

  uint64 cookie;
  GURL url;
  gfx::Rect dimensions;
  int disposition;
  bool user_gesture;
  std::string profile_name;
};

#if defined(OS_WIN) && !defined(USE_AURA)

struct Reposition_Params {
  HWND window;
  HWND window_insert_after;
  int left;
  int top;
  int width;
  int height;
  int flags;
  bool set_parent;
  HWND parent_window;
};

#endif  // defined(OS_WIN)

struct AutomationURLRequest {
  AutomationURLRequest();
  AutomationURLRequest(const std::string& url,
                       const std::string& method,
                       const std::string& referrer,
                       const std::string& extra_request_headers,
                       scoped_refptr<net::UploadData> upload_data,
                       int resource_type,
                       int load_flags);
  ~AutomationURLRequest();

  std::string url;
  std::string method;
  std::string referrer;
  std::string extra_request_headers;
  scoped_refptr<net::UploadData> upload_data;
  int resource_type;  // see webkit/glue/resource_type.h
  int load_flags; // see net/base/load_flags.h
};

namespace IPC {

template <>
struct ParamTraits<AutomationMsg_Find_Params> {
  typedef AutomationMsg_Find_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<AutomationMsg_NavigationResponseValues> {
  typedef AutomationMsg_NavigationResponseValues param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<AutomationMsg_ExtensionProperty> {
  typedef AutomationMsg_ExtensionProperty param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<SecurityStyle> {
  typedef SecurityStyle param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<PageType> {
  typedef PageType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

#if defined(OS_WIN) && !defined(USE_AURA)

// Traits for SetWindowPos_Params structure to pack/unpack.
template <>
struct ParamTraits<Reposition_Params> {
  typedef Reposition_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.window);
    WriteParam(m, p.window_insert_after);
    WriteParam(m, p.left);
    WriteParam(m, p.top);
    WriteParam(m, p.width);
    WriteParam(m, p.height);
    WriteParam(m, p.flags);
    WriteParam(m, p.set_parent);
    WriteParam(m, p.parent_window);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->window) &&
           ReadParam(m, iter, &p->window_insert_after) &&
           ReadParam(m, iter, &p->left) &&
           ReadParam(m, iter, &p->top) &&
           ReadParam(m, iter, &p->width) &&
           ReadParam(m, iter, &p->height) &&
           ReadParam(m, iter, &p->flags) &&
           ReadParam(m, iter, &p->set_parent) &&
           ReadParam(m, iter, &p->parent_window);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.window, l);
    l->append(", ");
    LogParam(p.window_insert_after, l);
    l->append(", ");
    LogParam(p.left, l);
    l->append(", ");
    LogParam(p.top, l);
    l->append(", ");
    LogParam(p.width, l);
    l->append(", ");
    LogParam(p.height, l);
    l->append(", ");
    LogParam(p.flags, l);
    l->append(", ");
    LogParam(p.set_parent, l);
    l->append(", ");
    LogParam(p.parent_window, l);
    l->append(")");
  }
};
#endif  // defined(OS_WIN)

// Traits for AutomationURLRequest structure to pack/unpack.
template <>
struct ParamTraits<AutomationURLRequest> {
  typedef AutomationURLRequest param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for AutomationURLResponse structure to pack/unpack.
template <>
struct ParamTraits<AutomationURLResponse> {
  typedef AutomationURLResponse param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for ExternalTabSettings structure to pack/unpack.
template <>
struct ParamTraits<ExternalTabSettings> {
  typedef ExternalTabSettings param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for NavigationInfo structure to pack/unpack.
template <>
struct ParamTraits<NavigationInfo> {
  typedef NavigationInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for MiniContextMenuParams structure to pack/unpack.
template <>
struct ParamTraits<MiniContextMenuParams> {
  typedef MiniContextMenuParams param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for ContextMenuModel structure to pack/unpack.
template <>
struct ParamTraits<ContextMenuModel> {
  typedef ContextMenuModel param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<AttachExternalTabParams> {
  typedef AttachExternalTabParams param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_AUTOMATION_MESSAGES_H__

// Keep this internal message file unchanged to preserve line numbering
// (and hence the dubious __LINE__-based message numberings) across versions.
#include "chrome/common/automation_messages_internal.h"
