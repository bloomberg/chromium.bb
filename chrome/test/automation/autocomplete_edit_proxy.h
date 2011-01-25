// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H_
#define CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H_
#pragma once

#include <string>
#include <vector>

#include "base/string_number_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/test/automation/automation_handle_tracker.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

// The purpose of this class is to act as a serializable version of
// AutocompleteMatch. The reason for this class is because we don't want to
// serialize all elements of AutocompleteMatch and we want some data from the
// autocomplete provider without the hassle of serializing it.
struct AutocompleteMatchData {
 public:
  AutocompleteMatchData()
      : relevance(0),
        deletable(false),
        inline_autocomplete_offset(0),
        is_history_what_you_typed_match(false),
        starred(false) {
  }

  explicit AutocompleteMatchData(const AutocompleteMatch& match)
      : provider_name(match.provider->name()),
        relevance(match.relevance),
        deletable(match.deletable),
        fill_into_edit(match.fill_into_edit),
        inline_autocomplete_offset(match.inline_autocomplete_offset),
        destination_url(match.destination_url),
        contents(match.contents),
        description(match.description),
        is_history_what_you_typed_match(match.is_history_what_you_typed_match),
        type(AutocompleteMatch::TypeToString(match.type)),
        starred(match.starred) {
  }

  std::string provider_name;
  int relevance;
  bool deletable;
  string16 fill_into_edit;
  size_t inline_autocomplete_offset;
  GURL destination_url;
  string16 contents;
  string16 description;
  bool is_history_what_you_typed_match;
  std::string type;
  bool starred;
};
typedef std::vector<AutocompleteMatchData> Matches;

namespace IPC {

template <>
struct ParamTraits<AutocompleteMatchData> {
  typedef AutocompleteMatchData param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString(p.provider_name);
    m->WriteInt(p.relevance);
    m->WriteBool(p.deletable);
    m->WriteString16(p.fill_into_edit);
    m->WriteSize(p.inline_autocomplete_offset);
    m->WriteString(p.destination_url.possibly_invalid_spec());
    m->WriteString16(p.contents);
    m->WriteString16(p.description);
    m->WriteBool(p.is_history_what_you_typed_match);
    m->WriteString(p.type);
    m->WriteBool(p.starred);
  }

  static bool Read(const Message* m, void** iter, param_type* r) {
    std::string destination_url;
    if (!m->ReadString(iter, &r->provider_name) ||
        !m->ReadInt(iter, &r->relevance) ||
        !m->ReadBool(iter, &r->deletable) ||
        !m->ReadString16(iter, &r->fill_into_edit) ||
        !m->ReadSize(iter, &r->inline_autocomplete_offset) ||
        !m->ReadString(iter, &destination_url) ||
        !m->ReadString16(iter, &r->contents) ||
        !m->ReadString16(iter, &r->description) ||
        !m->ReadBool(iter, &r->is_history_what_you_typed_match) ||
        !m->ReadString(iter, &r->type) ||
        !m->ReadBool(iter, &r->starred))
      return false;
    r->destination_url = GURL(destination_url);
    return true;
  }

  static void Log(const param_type& p, std::string* l) {
    l->append("[");
    l->append(p.provider_name);
    l->append(" ");
    l->append(base::IntToString(p.relevance));
    l->append(" ");
    l->append(p.deletable ? "true" : "false");
    l->append(" ");
    LogParam(p.fill_into_edit, l);
    l->append(" ");
    l->append(base::IntToString(p.inline_autocomplete_offset));
    l->append(" ");
    l->append(p.destination_url.spec());
    l->append(" ");
    LogParam(p.contents, l);
    l->append(" ");
    LogParam(p.description, l);
    l->append(" ");
    l->append(p.is_history_what_you_typed_match ? "true" : "false");
    l->append(" ");
    l->append(p.type);
    l->append(" ");
    l->append(p.starred ? "true" : "false");
    l->append("]");
  }
};
}  // namespace IPC

class AutocompleteEditProxy : public AutomationResourceProxy {
 public:
  AutocompleteEditProxy(AutomationMessageSender* sender,
                        AutomationHandleTracker* tracker,
                        int handle)
      : AutomationResourceProxy(tracker, sender, handle) {}
  virtual ~AutocompleteEditProxy() {}

  // All these functions return true if the autocomplete edit is valid and
  // there are no IPC errors.

  // Gets the text visible in the omnibox.
  bool GetText(string16* text) const;

  // Sets the text visible in the omnibox.
  bool SetText(const string16& text);

  // Determines if a query to an autocomplete provider is still in progress.
  // NOTE: No autocomplete queries will be made if the omnibox doesn't have
  // focus.  This can be achieved by sending a IDC_FOCUS_LOCATION accelerator
  // to the browser.
  bool IsQueryInProgress(bool* query_in_progress) const;

  // Gets a list of autocomplete matches that have been gathered so far.
  bool GetAutocompleteMatches(Matches* matches) const;

  // Waits for the autocomplete edit to receive focus.
  bool WaitForFocus() const;

  // Waits for all queries to autocomplete providers to complete.
  // |wait_timeout_ms| gives the number of milliseconds to wait for the query
  // to finish. Returns false if IPC call failed or if the function times out.
  bool WaitForQuery(int wait_timeout_ms) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditProxy);
};

#endif  // CHROME_TEST_AUTOMATION_AUTOCOMPLETE_EDIT_PROXY_H_
