// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/force_tls_state.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace net {

ForceTLSState::ForceTLSState()
    : delegate_(NULL) {
}

void ForceTLSState::DidReceiveHeader(const GURL& url,
                                     const std::string& value) {
  int max_age;
  bool include_subdomains;

  if (!ParseHeader(value, &max_age, &include_subdomains))
    return;

  base::Time current_time(base::Time::Now());
  base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age);
  base::Time expiry = current_time + max_age_delta;

  EnableHost(url.host(), expiry, include_subdomains);
}

void ForceTLSState::EnableHost(const std::string& host, base::Time expiry,
                               bool include_subdomains) {
  // TODO(abarth): Canonicalize host.
  AutoLock lock(lock_);

  State state = {expiry, include_subdomains};
  enabled_hosts_[host] = state;
  DirtyNotify();
}

bool ForceTLSState::IsEnabledForHost(const std::string& host) {
  // TODO(abarth): Canonicalize host.
  // TODO: check for subdomains too.

  AutoLock lock(lock_);
  std::map<std::string, State>::iterator i = enabled_hosts_.find(host);
  if (i == enabled_hosts_.end())
    return false;

  base::Time current_time(base::Time::Now());
  if (current_time > i->second.expiry) {
    enabled_hosts_.erase(i);
    DirtyNotify();
    return false;
  }

  return true;
}

// "X-Force-TLS" ":" "max-age" "=" delta-seconds *1INCLUDESUBDOMAINS
// INCLUDESUBDOMAINS = [ " includeSubDomains" ]
bool ForceTLSState::ParseHeader(const std::string& value,
                                int* max_age,
                                bool* include_subdomains) {
  DCHECK(max_age);
  DCHECK(include_subdomains);

  int max_age_candidate;

  enum ParserState {
    START,
    AFTER_MAX_AGE_LABEL,
    AFTER_MAX_AGE_EQUALS,
    AFTER_MAX_AGE,
    AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER,
    AFTER_INCLUDE_SUBDOMAINS,
  } state = START;

  StringTokenizer tokenizer(value, " =");
  tokenizer.set_options(StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    DCHECK(!tokenizer.token_is_delim() || tokenizer.token().length() == 1);
    DCHECK(tokenizer.token_is_delim() || *tokenizer.token_begin() != ' ');
    switch (state) {
      case START:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (!LowerCaseEqualsASCII(tokenizer.token(), "max-age"))
          return false;
        state = AFTER_MAX_AGE_LABEL;
        break;

      case AFTER_MAX_AGE_LABEL:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (*tokenizer.token_begin() != '=')
          return false;
        DCHECK(tokenizer.token().length() ==  1);
        state = AFTER_MAX_AGE_EQUALS;
        break;

      case AFTER_MAX_AGE_EQUALS:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (!StringToInt(tokenizer.token(), &max_age_candidate))
          return false;
        if (max_age_candidate < 0)
          return false;
        state = AFTER_MAX_AGE;
        break;

      case AFTER_MAX_AGE:
        if (*tokenizer.token_begin() != ' ')
          return false;
        state = AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER;
        break;

      case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (!LowerCaseEqualsASCII(tokenizer.token(), "includesubdomains"))
          return false;
        state = AFTER_INCLUDE_SUBDOMAINS;
        break;

      case AFTER_INCLUDE_SUBDOMAINS:
        if (*tokenizer.token_begin() != ' ')
          return false;
        break;

      default:
        NOTREACHED();
    }
  }

  // We've consumed all the input.  Let's see what state we ended up in.
  switch (state) {
    case START:
    case AFTER_MAX_AGE_LABEL:
    case AFTER_MAX_AGE_EQUALS:
      return false;
    case AFTER_MAX_AGE:
    case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
      *max_age = max_age_candidate;
      *include_subdomains = false;
      return true;
    case AFTER_INCLUDE_SUBDOMAINS:
      *max_age = max_age_candidate;
      *include_subdomains = true;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

void ForceTLSState::SetDelegate(ForceTLSState::Delegate* delegate) {
  AutoLock lock(lock_);

  delegate_ = delegate;
}

bool ForceTLSState::Serialise(std::string* output) {
  AutoLock lock(lock_);

  DictionaryValue toplevel;
  for (std::map<std::string, State>::const_iterator
       i = enabled_hosts_.begin(); i != enabled_hosts_.end(); ++i) {
    DictionaryValue* state = new DictionaryValue;
    state->SetBoolean(L"include_subdomains", i->second.include_subdomains);
    state->SetReal(L"expiry", i->second.expiry.ToDoubleT());

    toplevel.Set(ASCIIToWide(i->first), state);
  }

  JSONWriter::Write(&toplevel, true /* pretty print */, output);
  return true;
}

bool ForceTLSState::Deserialise(const std::string& input) {
  AutoLock lock(lock_);

  enabled_hosts_.clear();

  scoped_ptr<Value> value(
      JSONReader::Read(input, false /* do not allow trailing commas */));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* dict_value = reinterpret_cast<DictionaryValue*>(value.get());
  const base::Time current_time(base::Time::Now());

  for (DictionaryValue::key_iterator
       i = dict_value->begin_keys(); i != dict_value->end_keys(); ++i) {
    DictionaryValue* state;
    if (!dict_value->GetDictionary(*i, &state))
      continue;

    const std::string host = WideToASCII(*i);
    bool include_subdomains;
    double expiry;

    if (!state->GetBoolean(L"include_subdomains", &include_subdomains) ||
        !state->GetReal(L"expiry", &expiry)) {
      continue;
    }

    base::Time expiry_time = base::Time::FromDoubleT(expiry);
    if (expiry_time <= current_time)
      continue;

    State new_state = { expiry_time, include_subdomains };
    enabled_hosts_[host] = new_state;
  }

  return enabled_hosts_.size() > 0;
}

void ForceTLSState::DirtyNotify() {
  if (delegate_)
    delegate_->StateIsDirty(this);
}

}  // namespace
