// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define IPC_MESSAGE_IMPL
#include "chrome/common/automation_messages.h"

AutomationURLRequest::AutomationURLRequest()
    : resource_type(0),
      load_flags(0) {
}

AutomationURLRequest::AutomationURLRequest(
    const std::string& in_url,
    const std::string& in_method,
    const std::string& in_referrer,
    const std::string& in_extra_request_headers,
    scoped_refptr<net::UploadData> in_upload_data,
    int in_resource_type,
    int in_load_flags)
    : url(in_url),
      method(in_method),
      referrer(in_referrer),
      extra_request_headers(in_extra_request_headers),
      upload_data(in_upload_data),
      resource_type(in_resource_type),
      load_flags(in_load_flags) {
}

AutomationURLRequest::~AutomationURLRequest() {}

AutomationURLResponse::AutomationURLResponse()
    : content_length(0),
      redirect_status(0) {
}

AutomationURLResponse::AutomationURLResponse(
    const std::string& in_mime_type, const std::string& in_headers,
    int64 in_content_length, const base::Time& in_last_modified,
    const std::string& in_redirect_url, int in_redirect_status,
    const net::HostPortPair& host_socket_address)
    : mime_type(in_mime_type),
      headers(in_headers),
      content_length(in_content_length),
      last_modified(in_last_modified),
      redirect_url(in_redirect_url),
      redirect_status(in_redirect_status),
      socket_address(host_socket_address) {
}


AutomationURLResponse::~AutomationURLResponse() {}

ExternalTabSettings::ExternalTabSettings()
    : parent(NULL),
      dimensions(),
      style(0),
      is_off_the_record(false),
      load_requests_via_automation(false),
      handle_top_level_requests(false),
      initial_url(),
      referrer(),
      infobars_enabled(false),
      route_all_top_level_navigations(false) {
}

ExternalTabSettings::ExternalTabSettings(
    gfx::NativeWindow in_parent,
    const gfx::Rect& in_dimensions,
    unsigned int in_style,
    bool in_is_off_the_record,
    bool in_load_requests_via_automation,
    bool in_handle_top_level_requests,
    const GURL& in_initial_url,
    const GURL& in_referrer,
    bool in_infobars_enabled,
    bool in_route_all_top_level_navigations)
    : parent(in_parent),
      dimensions(in_dimensions),
      style(in_style),
      is_off_the_record(in_is_off_the_record),
      load_requests_via_automation(in_load_requests_via_automation),
      handle_top_level_requests(in_handle_top_level_requests),
      initial_url(in_initial_url),
      referrer(in_referrer),
      infobars_enabled(in_infobars_enabled),
      route_all_top_level_navigations(in_route_all_top_level_navigations) {
}

ExternalTabSettings::~ExternalTabSettings() {}

NavigationInfo::NavigationInfo()
    : navigation_type(0),
      relative_offset(0),
      navigation_index(0),
      displayed_insecure_content(0),
      ran_insecure_content(0) {
}

NavigationInfo::NavigationInfo(int in_navigation_type,
                               int in_relative_offset,
                               int in_navigation_index,
                               const std::wstring& in_title,
                               const GURL& in_url,
                               const GURL& in_referrer,
                               SecurityStyle in_security_style,
                               bool in_displayed_insecure_content,
                               bool in_ran_insecure_content)
    : navigation_type(in_navigation_type),
      relative_offset(in_relative_offset),
      navigation_index(in_navigation_index),
      title(in_title),
      url(in_url),
      referrer(in_referrer),
      security_style(in_security_style),
      displayed_insecure_content(in_displayed_insecure_content),
      ran_insecure_content(in_ran_insecure_content) {
}

NavigationInfo::~NavigationInfo() {}

MiniContextMenuParams::MiniContextMenuParams()
    : screen_x(0),
      screen_y(0) {
}

MiniContextMenuParams::MiniContextMenuParams(int in_screen_x,
                                             int in_screen_y,
                                             const GURL& in_link_url,
                                             const GURL& in_unfiltered_link_url,
                                             const GURL& in_src_url,
                                             const GURL& in_page_url,
                                             const GURL& in_frame_url)
    : screen_x(in_screen_x),
      screen_y(in_screen_y),
      link_url(in_link_url),
      unfiltered_link_url(in_unfiltered_link_url),
      src_url(in_src_url),
      page_url(in_page_url),
      frame_url(in_frame_url) {
}

MiniContextMenuParams::~MiniContextMenuParams() {}

AttachExternalTabParams::AttachExternalTabParams()
    : cookie(0),
      disposition(0),
      user_gesture(false) {
}

AttachExternalTabParams::AttachExternalTabParams(
    uint64 in_cookie,
    const GURL& in_url,
    const gfx::Rect& in_dimensions,
    int in_disposition,
    bool in_user_gesture,
    const std::string& in_profile_name)
    : cookie(in_cookie),
      url(in_url),
      dimensions(in_dimensions),
      disposition(in_disposition),
      user_gesture(in_user_gesture),
      profile_name(in_profile_name) {
}

AttachExternalTabParams::~AttachExternalTabParams() {}

namespace IPC {

// static
void ParamTraits<AutomationMsg_Find_Params>::Write(Message* m,
                                                   const param_type& p) {
  WriteParam(m, p.unused);
  WriteParam(m, p.search_string);
  WriteParam(m, p.forward);
  WriteParam(m, p.match_case);
  WriteParam(m, p.find_next);
}

// static
bool ParamTraits<AutomationMsg_Find_Params>::Read(const Message* m,
                                                  void** iter,
                                                  param_type* p) {
  return
      ReadParam(m, iter, &p->unused) &&
      ReadParam(m, iter, &p->search_string) &&
      ReadParam(m, iter, &p->forward) &&
      ReadParam(m, iter, &p->match_case) &&
      ReadParam(m, iter, &p->find_next);
}

// static
void ParamTraits<AutomationMsg_Find_Params>::Log(const param_type& p,
                                                 std::string* l) {
  l->append("<AutomationMsg_Find_Params>");
}

// static
void ParamTraits<AutomationMsg_NavigationResponseValues>::Write(
    Message* m,
    const param_type& p) {
  m->WriteInt(p);
}

// static
bool ParamTraits<AutomationMsg_NavigationResponseValues>::Read(const Message* m,
                                                               void** iter,
                                                               param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<AutomationMsg_NavigationResponseValues>(type);
  return true;
}

// static
void ParamTraits<AutomationMsg_NavigationResponseValues>::Log(
    const param_type& p, std::string* l) {
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

// static
void ParamTraits<AutomationMsg_ExtensionResponseValues>::Write(
    Message* m,
    const param_type& p) {
  m->WriteInt(p);
}

// static
bool ParamTraits<AutomationMsg_ExtensionResponseValues>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<AutomationMsg_ExtensionResponseValues>(type);
  return true;
}

// static
void ParamTraits<AutomationMsg_ExtensionResponseValues>::Log(
    const param_type& p,
    std::string* l) {
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

// static
void ParamTraits<AutomationMsg_ExtensionProperty>::Write(Message* m,
                                                         const param_type& p) {
  m->WriteInt(p);
}

// static
bool ParamTraits<AutomationMsg_ExtensionProperty>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<AutomationMsg_ExtensionProperty>(type);
  return true;
}

// static
void ParamTraits<AutomationMsg_ExtensionProperty>::Log(const param_type& p,
                                                       std::string* l) {
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

// static
void ParamTraits<SecurityStyle>::Write(Message* m, const param_type& p) {
  m->WriteInt(p);
}

// static
bool ParamTraits<SecurityStyle>::Read(const Message* m,
                                      void** iter,
                                      param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<SecurityStyle>(type);
  return true;
}

// static
void ParamTraits<SecurityStyle>::Log(const param_type& p, std::string* l) {
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

// static
void ParamTraits<PageType>::Write(Message* m, const param_type& p) {
  m->WriteInt(p);
}

// static
bool ParamTraits<PageType>::Read(const Message* m, void** iter, param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<PageType>(type);
  return true;
}

// static
void ParamTraits<PageType>::Log(const param_type& p, std::string* l) {
  std::string control;
  switch (p) {
    case NORMAL_PAGE:
      control = "NORMAL_PAGE";
      break;
    case ERROR_PAGE:
      control = "ERROR_PAGE";
      break;
    case INTERSTITIAL_PAGE:
      control = "INTERSTITIAL_PAGE";
      break;
    default:
      control = "UNKNOWN";
      break;
  }

  LogParam(control, l);
}

// static
void ParamTraits<AutomationURLRequest>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.method);
  WriteParam(m, p.referrer);
  WriteParam(m, p.extra_request_headers);
  WriteParam(m, p.upload_data);
  WriteParam(m, p.resource_type);
  WriteParam(m, p.load_flags);
}

// static
bool ParamTraits<AutomationURLRequest>::Read(const Message* m,
                                             void** iter,
                                             param_type* p) {
  return ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->extra_request_headers) &&
      ReadParam(m, iter, &p->upload_data) &&
      ReadParam(m, iter, &p->resource_type) &&
      ReadParam(m, iter, &p->load_flags);
}

// static
void ParamTraits<AutomationURLRequest>::Log(const param_type& p,
                                            std::string* l) {
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
  l->append(", ");
  LogParam(p.resource_type, l);
  l->append(", ");
  LogParam(p.load_flags, l);
  l->append(")");
}

// static
void ParamTraits<AutomationURLResponse>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.mime_type);
  WriteParam(m, p.headers);
  WriteParam(m, p.content_length);
  WriteParam(m, p.last_modified);
  WriteParam(m, p.redirect_url);
  WriteParam(m, p.redirect_status);
  WriteParam(m, p.socket_address);
}

// static
bool ParamTraits<AutomationURLResponse>::Read(const Message* m,
                                              void** iter,
                                              param_type* p) {
  return ReadParam(m, iter, &p->mime_type) &&
      ReadParam(m, iter, &p->headers) &&
      ReadParam(m, iter, &p->content_length) &&
      ReadParam(m, iter, &p->last_modified) &&
      ReadParam(m, iter, &p->redirect_url) &&
      ReadParam(m, iter, &p->redirect_status) &&
      ReadParam(m, iter, &p->socket_address);
}

// static
void ParamTraits<AutomationURLResponse>::Log(const param_type& p,
                                             std::string* l) {
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
  l->append(", ");
  LogParam(p.socket_address, l);
  l->append(")");
}

// static
void ParamTraits<ExternalTabSettings>::Write(Message* m,
                                             const param_type& p) {
  WriteParam(m, p.parent);
  WriteParam(m, p.dimensions);
  WriteParam(m, p.style);
  WriteParam(m, p.is_off_the_record);
  WriteParam(m, p.load_requests_via_automation);
  WriteParam(m, p.handle_top_level_requests);
  WriteParam(m, p.initial_url);
  WriteParam(m, p.referrer);
  WriteParam(m, p.infobars_enabled);
  WriteParam(m, p.route_all_top_level_navigations);
}

// static
bool ParamTraits<ExternalTabSettings>::Read(const Message* m,
                                            void** iter,
                                            param_type* p) {
  return ReadParam(m, iter, &p->parent) &&
      ReadParam(m, iter, &p->dimensions) &&
      ReadParam(m, iter, &p->style) &&
      ReadParam(m, iter, &p->is_off_the_record) &&
      ReadParam(m, iter, &p->load_requests_via_automation) &&
      ReadParam(m, iter, &p->handle_top_level_requests) &&
      ReadParam(m, iter, &p->initial_url) &&
      ReadParam(m, iter, &p->referrer) &&
      ReadParam(m, iter, &p->infobars_enabled) &&
      ReadParam(m, iter, &p->route_all_top_level_navigations);
}

// static
void ParamTraits<ExternalTabSettings>::Log(const param_type& p,
                                           std::string* l) {
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
  l->append(", ");
  LogParam(p.route_all_top_level_navigations, l);
  l->append(")");
}

// static
void ParamTraits<NavigationInfo>::Write(Message* m, const param_type& p) {
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

// static
bool ParamTraits<NavigationInfo>::Read(const Message* m,
                                       void** iter,
                                       param_type* p) {
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

// static
void ParamTraits<NavigationInfo>::Log(const param_type& p, std::string* l) {
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

// static
void ParamTraits<MiniContextMenuParams>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.screen_x);
  WriteParam(m, p.screen_y);
  WriteParam(m, p.link_url);
  WriteParam(m, p.unfiltered_link_url);
  WriteParam(m, p.src_url);
  WriteParam(m, p.page_url);
  WriteParam(m, p.frame_url);
}

// static
bool ParamTraits<MiniContextMenuParams>::Read(const Message* m,
                                              void** iter,
                                              param_type* p) {
  return ReadParam(m, iter, &p->screen_x) &&
      ReadParam(m, iter, &p->screen_y) &&
      ReadParam(m, iter, &p->link_url) &&
      ReadParam(m, iter, &p->unfiltered_link_url) &&
      ReadParam(m, iter, &p->src_url) &&
      ReadParam(m, iter, &p->page_url) &&
      ReadParam(m, iter, &p->frame_url);
}

// static
void ParamTraits<MiniContextMenuParams>::Log(const param_type& p,
                                             std::string* l) {
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

// static
void ParamTraits<AttachExternalTabParams>::Write(Message* m,
                                                 const param_type& p) {
  WriteParam(m, p.cookie);
  WriteParam(m, p.url);
  WriteParam(m, p.dimensions);
  WriteParam(m, p.disposition);
  WriteParam(m, p.user_gesture);
  WriteParam(m, p.profile_name);
}

// static
bool ParamTraits<AttachExternalTabParams>::Read(const Message* m,
                                                void** iter,
                                                param_type* p) {
  return ReadParam(m, iter, &p->cookie) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->dimensions) &&
      ReadParam(m, iter, &p->disposition) &&
      ReadParam(m, iter, &p->user_gesture) &&
      ReadParam(m, iter, &p->profile_name);
}

// static
void ParamTraits<AttachExternalTabParams>::Log(const param_type& p,
                                               std::string* l) {
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

}  // namespace IPC
