// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__

#include <string>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/test/automation/automation_constants.h"
#include "net/base/upload_data.h"


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

namespace IPC {

template <>
struct ParamTraits<AutomationMsg_Find_Params> {
  typedef AutomationMsg_Find_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.unused);
    WriteParam(m, p.search_string);
    WriteParam(m, p.forward);
    WriteParam(m, p.match_case);
    WriteParam(m, p.find_next);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->unused) &&
      ReadParam(m, iter, &p->search_string) &&
      ReadParam(m, iter, &p->forward) &&
      ReadParam(m, iter, &p->match_case) &&
      ReadParam(m, iter, &p->find_next);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<AutomationMsg_Find_Params>");
  }
};

template <>
struct ParamTraits<AutomationMsg_NavigationResponseValues> {
  typedef AutomationMsg_NavigationResponseValues param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AutomationMsg_NavigationResponseValues>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case AUTOMATION_MSG_NAVIGATION_ERROR:
      control = L"AUTOMATION_MSG_NAVIGATION_ERROR";
      break;
     case AUTOMATION_MSG_NAVIGATION_SUCCESS:
      control = L"AUTOMATION_MSG_NAVIGATION_SUCCESS";
      break;
     case AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED:
      control = L"AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

template <>
struct ParamTraits<AutomationMsg_ExtensionResponseValues> {
  typedef AutomationMsg_ExtensionResponseValues param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AutomationMsg_ExtensionResponseValues>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED:
      control = L"AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED";
      break;
     case AUTOMATION_MSG_EXTENSION_INSTALL_FAILED:
      control = L"AUTOMATION_MSG_EXTENSION_INSTALL_FAILED";
      break;
     case AUTOMATION_MSG_EXTENSION_ALREADY_INSTALLED:
      control = L"AUTOMATION_MSG_EXTENSION_ALREADY_INSTALLED";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

template <>
struct ParamTraits<SecurityStyle> {
  typedef SecurityStyle param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<SecurityStyle>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case SECURITY_STYLE_UNKNOWN:
      control = L"SECURITY_STYLE_UNKNOWN";
      break;
     case SECURITY_STYLE_UNAUTHENTICATED:
      control = L"SECURITY_STYLE_UNAUTHENTICATED";
      break;
     case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      control = L"SECURITY_STYLE_AUTHENTICATION_BROKEN";
      break;
     case SECURITY_STYLE_AUTHENTICATED:
      control = L"SECURITY_STYLE_AUTHENTICATED";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

template <>
struct ParamTraits<NavigationEntry::PageType> {
  typedef NavigationEntry::PageType param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<NavigationEntry::PageType>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring control;
    switch (p) {
     case NavigationEntry::NORMAL_PAGE:
      control = L"NORMAL_PAGE";
      break;
     case NavigationEntry::ERROR_PAGE:
      control = L"ERROR_PAGE";
      break;
     case NavigationEntry::INTERSTITIAL_PAGE:
      control = L"INTERSTITIAL_PAGE";
      break;
     default:
      control = L"UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

#if defined(OS_WIN)
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
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.window, l);
    l->append(L", ");
    LogParam(p.window_insert_after, l);
    l->append(L", ");
    LogParam(p.left, l);
    l->append(L", ");
    LogParam(p.top, l);
    l->append(L", ");
    LogParam(p.width, l);
    l->append(L", ");
    LogParam(p.height, l);
    l->append(L", ");
    LogParam(p.flags, l);
    l->append(L", ");
    LogParam(p.set_parent, l);
    l->append(L", ");
    LogParam(p.parent_window, l);
    l->append(L")");
  }
};
#endif  // defined(OS_WIN)

struct AutomationURLRequest {
  std::string url;
  std::string method;
  std::string referrer;
  std::string extra_request_headers;
  scoped_refptr<net::UploadData> upload_data;
};

// Traits for AutomationURLRequest structure to pack/unpack.
template <>
struct ParamTraits<AutomationURLRequest> {
  typedef AutomationURLRequest param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.method);
    WriteParam(m, p.referrer);
    WriteParam(m, p.extra_request_headers);
    WriteParam(m, p.upload_data);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->url) &&
           ReadParam(m, iter, &p->method) &&
           ReadParam(m, iter, &p->referrer) &&
           ReadParam(m, iter, &p->extra_request_headers) &&
           ReadParam(m, iter, &p->upload_data);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.method, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L", ");
    LogParam(p.extra_request_headers, l);
    l->append(L", ");
    LogParam(p.upload_data, l);
    l->append(L")");
  }
};

struct AutomationURLResponse {
  std::string mime_type;
  std::string headers;
  int64 content_length;
  base::Time last_modified;
  std::string persistent_cookies;
  std::string redirect_url;
  int redirect_status;
};

// Traits for AutomationURLResponse structure to pack/unpack.
template <>
struct ParamTraits<AutomationURLResponse> {
  typedef AutomationURLResponse param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.mime_type);
    WriteParam(m, p.headers);
    WriteParam(m, p.content_length);
    WriteParam(m, p.last_modified);
    WriteParam(m, p.persistent_cookies);
    WriteParam(m, p.redirect_url);
    WriteParam(m, p.redirect_status);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->mime_type) &&
           ReadParam(m, iter, &p->headers) &&
           ReadParam(m, iter, &p->content_length) &&
           ReadParam(m, iter, &p->last_modified) &&
           ReadParam(m, iter, &p->persistent_cookies) &&
           ReadParam(m, iter, &p->redirect_url) &&
           ReadParam(m, iter, &p->redirect_status);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.mime_type, l);
    l->append(L", ");
    LogParam(p.headers, l);
    l->append(L", ");
    LogParam(p.content_length, l);
    l->append(L", ");
    LogParam(p.last_modified, l);
    l->append(L", ");
    LogParam(p.persistent_cookies, l);
    l->append(L", ");
    LogParam(p.redirect_url, l);
    l->append(L", ");
    LogParam(p.redirect_status, l);
    l->append(L")");
  }
};

struct ExternalTabSettings {
  gfx::NativeWindow parent;
  gfx::Rect dimensions;
  unsigned int style;
  bool is_off_the_record;
  bool load_requests_via_automation;
  bool handle_top_level_requests;
  GURL initial_url;
  GURL referrer;
};

// Traits for ExternalTabSettings structure to pack/unpack.
template <>
struct ParamTraits<ExternalTabSettings> {
  typedef ExternalTabSettings param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.parent);
    WriteParam(m, p.dimensions);
    WriteParam(m, p.style);
    WriteParam(m, p.is_off_the_record);
    WriteParam(m, p.load_requests_via_automation);
    WriteParam(m, p.handle_top_level_requests);
    WriteParam(m, p.initial_url);
    WriteParam(m, p.referrer);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->parent) &&
           ReadParam(m, iter, &p->dimensions) &&
           ReadParam(m, iter, &p->style) &&
           ReadParam(m, iter, &p->is_off_the_record) &&
           ReadParam(m, iter, &p->load_requests_via_automation) &&
           ReadParam(m, iter, &p->handle_top_level_requests) &&
           ReadParam(m, iter, &p->initial_url) &&
           ReadParam(m, iter, &p->referrer);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.parent, l);
    l->append(L", ");
    LogParam(p.dimensions, l);
    l->append(L", ");
    LogParam(p.style, l);
    l->append(L", ");
    LogParam(p.is_off_the_record, l);
    l->append(L", ");
    LogParam(p.load_requests_via_automation, l);
    l->append(L", ");
    LogParam(p.handle_top_level_requests, l);
    l->append(L", ");
    LogParam(p.initial_url, l);
    l->append(L", ");
    LogParam(p.referrer, l);
    l->append(L")");
  }
};

struct NavigationInfo {
  int navigation_type;
  int relative_offset;
  int navigation_index;
  std::wstring title;
  GURL url;
  SecurityStyle security_style;
  bool has_mixed_content;
};

// Traits for NavigationInfo structure to pack/unpack.
template <>
struct ParamTraits<NavigationInfo> {
  typedef NavigationInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.navigation_type);
    WriteParam(m, p.relative_offset);
    WriteParam(m, p.navigation_index);
    WriteParam(m, p.title);
    WriteParam(m, p.url);
    WriteParam(m, p.security_style);
    WriteParam(m, p.has_mixed_content);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->navigation_type) &&
           ReadParam(m, iter, &p->relative_offset) &&
           ReadParam(m, iter, &p->navigation_index) &&
           ReadParam(m, iter, &p->title) &&
           ReadParam(m, iter, &p->url) &&
           ReadParam(m, iter, &p->security_style) &&
           ReadParam(m, iter, &p->has_mixed_content);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.navigation_type, l);
    l->append(L", ");
    LogParam(p.relative_offset, l);
    l->append(L", ");
    LogParam(p.navigation_index, l);
    l->append(L", ");
    LogParam(p.title, l);
    l->append(L", ");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.security_style, l);
    l->append(L", ");
    LogParam(p.has_mixed_content, l);
    l->append(L")");
  }
};

// A stripped down version of ContextMenuParams in webkit/glue/context_menu.h.
struct ContextMenuParams {
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

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;
};

// Traits for ContextMenuParams structure to pack/unpack.
template <>
struct ParamTraits<ContextMenuParams> {
  typedef ContextMenuParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.screen_x);
    WriteParam(m, p.screen_y);
    WriteParam(m, p.link_url);
    WriteParam(m, p.unfiltered_link_url);
    WriteParam(m, p.src_url);
    WriteParam(m, p.page_url);
    WriteParam(m, p.frame_url);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->screen_x) &&
      ReadParam(m, iter, &p->screen_y) &&
      ReadParam(m, iter, &p->link_url) &&
      ReadParam(m, iter, &p->unfiltered_link_url) &&
      ReadParam(m, iter, &p->src_url) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->frame_url);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.screen_x, l);
    l->append(L", ");
    LogParam(p.screen_y, l);
    l->append(L", ");
    LogParam(p.link_url, l);
    l->append(L", ");
    LogParam(p.unfiltered_link_url, l);
    l->append(L", ");
    LogParam(p.src_url, l);
    l->append(L", ");
    LogParam(p.page_url, l);
    l->append(L", ");
    LogParam(p.frame_url, l);
    l->append(L")");
  }
};


}  // namespace IPC

#define MESSAGES_INTERNAL_FILE \
    "chrome/test/automation/automation_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
