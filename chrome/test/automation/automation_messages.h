// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/test/automation/automation_constants.h"
#include "gfx/rect.h"
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
  static void Log(const param_type& p, std::string* l) {
    l->append("<AutomationMsg_Find_Params>");
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
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
     case AUTOMATION_MSG_NAVIGATION_ERROR:
      control = "AUTOMATION_MSG_NAVIGATION_ERROR";
      break;
     case AUTOMATION_MSG_NAVIGATION_SUCCESS:
      control = "AUTOMATION_MSG_NAVIGATION_SUCCESS";
      break;
     case AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED:
      control = "AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED";
      break;
     default:
      control = "UNKNOWN";
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
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
     case AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED:
      control = "AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED";
      break;
     case AUTOMATION_MSG_EXTENSION_INSTALL_FAILED:
      control = "AUTOMATION_MSG_EXTENSION_INSTALL_FAILED";
      break;
     default:
      control = "UNKNOWN";
      break;
    }

    LogParam(control, l);
  }
};

template <>
struct ParamTraits<AutomationMsg_ExtensionProperty> {
  typedef AutomationMsg_ExtensionProperty param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<AutomationMsg_ExtensionProperty>(type);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
     case AUTOMATION_MSG_EXTENSION_ID:
      control = "AUTOMATION_MSG_EXTENSION_ID";
      break;
     case AUTOMATION_MSG_EXTENSION_NAME:
      control = "AUTOMATION_MSG_EXTENSION_NAME";
      break;
     case AUTOMATION_MSG_EXTENSION_VERSION:
      control = "AUTOMATION_MSG_EXTENSION_VERSION";
      break;
     case AUTOMATION_MSG_EXTENSION_BROWSER_ACTION_INDEX:
      control = "AUTOMATION_MSG_EXTENSION_BROWSER_ACTION_INDEX";
      break;
     default:
      control = "UNKNOWN";
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
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
     case SECURITY_STYLE_UNKNOWN:
      control = "SECURITY_STYLE_UNKNOWN";
      break;
     case SECURITY_STYLE_UNAUTHENTICATED:
      control = "SECURITY_STYLE_UNAUTHENTICATED";
      break;
     case SECURITY_STYLE_AUTHENTICATION_BROKEN:
      control = "SECURITY_STYLE_AUTHENTICATION_BROKEN";
      break;
     case SECURITY_STYLE_AUTHENTICATED:
      control = "SECURITY_STYLE_AUTHENTICATED";
      break;
     default:
      control = "UNKNOWN";
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
  static void Log(const param_type& p, std::string* l) {
    std::string control;
    switch (p) {
     case NavigationEntry::NORMAL_PAGE:
      control = "NORMAL_PAGE";
      break;
     case NavigationEntry::ERROR_PAGE:
      control = "ERROR_PAGE";
      break;
     case NavigationEntry::INTERSTITIAL_PAGE:
      control = "INTERSTITIAL_PAGE";
      break;
     default:
      control = "UNKNOWN";
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
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.url, l);
    l->append(", ");
    LogParam(p.method, l);
    l->append(", ");
    LogParam(p.referrer, l);
    l->append(", ");
    LogParam(p.extra_request_headers, l);
    l->append(", ");
    LogParam(p.upload_data, l);
    l->append(")");
  }
};

struct AutomationURLResponse {
  std::string mime_type;
  std::string headers;
  int64 content_length;
  base::Time last_modified;
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
    WriteParam(m, p.redirect_url);
    WriteParam(m, p.redirect_status);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->mime_type) &&
           ReadParam(m, iter, &p->headers) &&
           ReadParam(m, iter, &p->content_length) &&
           ReadParam(m, iter, &p->last_modified) &&
           ReadParam(m, iter, &p->redirect_url) &&
           ReadParam(m, iter, &p->redirect_status);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.mime_type, l);
    l->append(", ");
    LogParam(p.headers, l);
    l->append(", ");
    LogParam(p.content_length, l);
    l->append(", ");
    LogParam(p.last_modified, l);
    l->append(", ");
    LogParam(p.redirect_url, l);
    l->append(", ");
    LogParam(p.redirect_status, l);
    l->append(")");
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
  bool infobars_enabled;
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
    WriteParam(m, p.infobars_enabled);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->parent) &&
           ReadParam(m, iter, &p->dimensions) &&
           ReadParam(m, iter, &p->style) &&
           ReadParam(m, iter, &p->is_off_the_record) &&
           ReadParam(m, iter, &p->load_requests_via_automation) &&
           ReadParam(m, iter, &p->handle_top_level_requests) &&
           ReadParam(m, iter, &p->initial_url) &&
           ReadParam(m, iter, &p->referrer) &&
           ReadParam(m, iter, &p->infobars_enabled);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.parent, l);
    l->append(", ");
    LogParam(p.dimensions, l);
    l->append(", ");
    LogParam(p.style, l);
    l->append(", ");
    LogParam(p.is_off_the_record, l);
    l->append(", ");
    LogParam(p.load_requests_via_automation, l);
    l->append(", ");
    LogParam(p.handle_top_level_requests, l);
    l->append(", ");
    LogParam(p.initial_url, l);
    l->append(", ");
    LogParam(p.referrer, l);
    l->append(", ");
    LogParam(p.infobars_enabled, l);
    l->append(")");
  }
};

struct NavigationInfo {
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
    WriteParam(m, p.referrer);
    WriteParam(m, p.security_style);
    WriteParam(m, p.displayed_insecure_content);
    WriteParam(m, p.ran_insecure_content);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->navigation_type) &&
           ReadParam(m, iter, &p->relative_offset) &&
           ReadParam(m, iter, &p->navigation_index) &&
           ReadParam(m, iter, &p->title) &&
           ReadParam(m, iter, &p->url) &&
           ReadParam(m, iter, &p->referrer) &&
           ReadParam(m, iter, &p->security_style) &&
           ReadParam(m, iter, &p->displayed_insecure_content) &&
           ReadParam(m, iter, &p->ran_insecure_content);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.navigation_type, l);
    l->append(", ");
    LogParam(p.relative_offset, l);
    l->append(", ");
    LogParam(p.navigation_index, l);
    l->append(", ");
    LogParam(p.title, l);
    l->append(", ");
    LogParam(p.url, l);
    l->append(", ");
    LogParam(p.referrer, l);
    l->append(", ");
    LogParam(p.security_style, l);
    l->append(", ");
    LogParam(p.displayed_insecure_content, l);
    l->append(", ");
    LogParam(p.ran_insecure_content, l);
    l->append(")");
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
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.screen_x, l);
    l->append(", ");
    LogParam(p.screen_y, l);
    l->append(", ");
    LogParam(p.link_url, l);
    l->append(", ");
    LogParam(p.unfiltered_link_url, l);
    l->append(", ");
    LogParam(p.src_url, l);
    l->append(", ");
    LogParam(p.page_url, l);
    l->append(", ");
    LogParam(p.frame_url, l);
    l->append(")");
  }
};

struct AttachExternalTabParams {
  uint64 cookie;
  GURL url;
  gfx::Rect dimensions;
  int disposition;
  bool user_gesture;
  std::string profile_name;
};

template <>
struct ParamTraits<AttachExternalTabParams> {
  typedef AttachExternalTabParams param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.cookie);
    WriteParam(m, p.url);
    WriteParam(m, p.dimensions);
    WriteParam(m, p.disposition);
    WriteParam(m, p.user_gesture);
    WriteParam(m, p.profile_name);
  }

  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->cookie) &&
        ReadParam(m, iter, &p->url) &&
        ReadParam(m, iter, &p->dimensions) &&
        ReadParam(m, iter, &p->disposition) &&
        ReadParam(m, iter, &p->user_gesture) &&
        ReadParam(m, iter, &p->profile_name);
  }

  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.cookie, l);
    l->append(", ");
    LogParam(p.url, l);
    l->append(", ");
    LogParam(p.dimensions, l);
    l->append(", ");
    LogParam(p.disposition, l);
    l->append(", ");
    LogParam(p.user_gesture, l);
    l->append(",");
    LogParam(p.profile_name, l);
    l->append(")");
  }
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE \
    "chrome/test/automation/automation_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_MESSAGES_H__
