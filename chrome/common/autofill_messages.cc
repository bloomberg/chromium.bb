// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"
#include "content/common/common_param_traits.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/autofill_messages.h"

namespace IPC {

void ParamTraits<webkit_glue::FormField>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.label());
  WriteParam(m, p.name());
  WriteParam(m, p.value());
  WriteParam(m, p.form_control_type());
  WriteParam(m, p.max_length());
  WriteParam(m, p.is_autofilled());
  WriteParam(m, p.option_strings());
}

bool ParamTraits<webkit_glue::FormField>::Read(const Message* m, void** iter,
                                               param_type* p) {
  string16 label, name, value, form_control_type;
  int max_length = 0;
  bool is_autofilled;
  std::vector<string16> options;
  bool result = ReadParam(m, iter, &label);
  result = result && ReadParam(m, iter, &name);
  result = result && ReadParam(m, iter, &value);
  result = result && ReadParam(m, iter, &form_control_type);
  result = result && ReadParam(m, iter, &max_length);
  result = result && ReadParam(m, iter, &is_autofilled);
  result = result && ReadParam(m, iter, &options);
  if (!result)
    return false;

  p->set_label(label);
  p->set_name(name);
  p->set_value(value);
  p->set_form_control_type(form_control_type);
  p->set_max_length(max_length);
  p->set_autofilled(is_autofilled);
  p->set_option_strings(options);
  return true;
}

void ParamTraits<webkit_glue::FormField>::Log(const param_type& p,
                                              std::string* l) {
  l->append("<FormField>");
}

void ParamTraits<webkit_glue::FormData>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.method);
  WriteParam(m, p.origin);
  WriteParam(m, p.action);
  WriteParam(m, p.user_submitted);
  WriteParam(m, p.fields);
}

bool ParamTraits<webkit_glue::FormData>::Read(const Message* m, void** iter,
                                              param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->method) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->action) &&
      ReadParam(m, iter, &p->user_submitted) &&
      ReadParam(m, iter, &p->fields);
}

void ParamTraits<webkit_glue::FormData>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<FormData>");
}

void ParamTraits<webkit_glue::PasswordFormFillData>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.basic_data);
  WriteParam(m, p.additional_logins);
  WriteParam(m, p.wait_for_username);
}

bool ParamTraits<webkit_glue::PasswordFormFillData>::Read(
    const Message* m, void** iter, param_type* r) {
  return
      ReadParam(m, iter, &r->basic_data) &&
      ReadParam(m, iter, &r->additional_logins) &&
      ReadParam(m, iter, &r->wait_for_username);
}

void ParamTraits<webkit_glue::PasswordFormFillData>::Log(const param_type& p,
                                                         std::string* l) {
  l->append("<PasswordFormFillData>");
}

}  // namespace IPC
