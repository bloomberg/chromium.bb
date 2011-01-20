// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_HELPER_H_
#define CHROME_RENDERER_AUTOFILL_HELPER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/task.h"
#include "chrome/renderer/form_manager.h"
#include "chrome/renderer/page_click_listener.h"
#include "chrome/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutoFillClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"

class PasswordAutocompleteManager;

// AutoFillHelper deals with AutoFill related communications between WebKit and
// the browser.  There is one AutofillHelper per RenderView.
// This code was originally part of RenderView.
// Note that AutoFill encompasses:
// - single text field suggestions, that we usually refer to as Autocomplete
// - entire form fill based on one field entry, referred to as form AutoFill.

class AutoFillHelper : public RenderViewObserver,
                       public PageClickListener,
                       public WebKit::WebAutoFillClient {
 public:
  // PasswordAutocompleteManager is guaranteed to outlive AutoFillHelper.
  AutoFillHelper(RenderView* render_view,
                 PasswordAutocompleteManager* password_autocomplete_manager);

  // WebKit::WebAutoFillClient implementation.  Public for tests.
  virtual void didAcceptAutoFillSuggestion(const WebKit::WebNode& node,
                                           const WebKit::WebString& value,
                                           const WebKit::WebString& label,
                                           int unique_id,
                                           unsigned index);
  virtual void didSelectAutoFillSuggestion(const WebKit::WebNode& node,
                                           const WebKit::WebString& value,
                                           const WebKit::WebString& label,
                                           int unique_id);
  virtual void didClearAutoFillSelection(const WebKit::WebNode& node);
  virtual void removeAutocompleteSuggestion(const WebKit::WebString& name,
                                            const WebKit::WebString& value);
  virtual void textFieldDidEndEditing(const WebKit::WebInputElement& element);
  virtual void textFieldDidChange(const WebKit::WebInputElement& element);
  virtual void textFieldDidReceiveKeyDown(
      const WebKit::WebInputElement& element,
      const WebKit::WebKeyboardEvent& event);

 private:
  enum AutoFillAction {
    AUTOFILL_NONE,     // No state set.
    AUTOFILL_FILL,     // Fill the AutoFill form data.
    AUTOFILL_PREVIEW,  // Preview the AutoFill form data.
  };

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void FrameDetached(WebKit::WebFrame* frame);
  virtual void FrameWillClose(WebKit::WebFrame* frame);
  virtual void FrameTranslated(WebKit::WebFrame* frame);

  // PageClickListener implementation:
  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused);

  void OnSuggestionsReturned(int query_id,
                             const std::vector<string16>& values,
                             const std::vector<string16>& labels,
                             const std::vector<string16>& icons,
                             const std::vector<int>& unique_ids);
  void OnFormDataFilled(int query_id, const webkit_glue::FormData& form);

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976
  void TextFieldDidChangeImpl(const WebKit::WebInputElement& element);

  // Shows the autocomplete suggestions for |element|.
  // This call is asynchronous and may or may not lead to the showing of a
  // suggestion popup (no popup is shown if there are no available suggestions).
  // |autofill_on_empty_values| specifies whether suggestions should be shown
  // when |element| contains no text.
  // |requires_caret_at_end| specifies whether suggestions should be shown when
  // the caret is not after the last character in |element|.
  // |display_warning_if_disabled| specifies whether a warning should be
  // displayed to the user if AutoFill has suggestions available, but cannot
  // fill them because it is disabled (e.g. when trying to fill a credit card
  // form on a non-secure website).
  void ShowSuggestions(const WebKit::WebInputElement& element,
                       bool autofill_on_empty_values,
                       bool requires_caret_at_end,
                       bool display_warning_if_disabled);

  // Queries the browser for Autocomplete and AutoFill suggestions for the given
  // |node|.
  void QueryAutoFillSuggestions(const WebKit::WebNode& node,
                                bool display_warning_if_disabled);

  // Queries the AutoFillManager for form data for the form containing |node|.
  // |value| is the current text in the field, and |unique_id| is the selected
  // profile's unique ID.  |action| specifies whether to Fill or Preview the
  // values returned from the AutoFillManager.
  void FillAutoFillFormData(const WebKit::WebNode& node,
                            int unique_id,
                            AutoFillAction action);

  // Scans the given frame for forms and sends them up to the browser.
  void SendForms(WebKit::WebFrame* frame);

  // Fills |form| and |field| with the FormData and FormField corresponding to
  // |node|. Returns true if the data was found; and false otherwise.
  bool FindFormAndFieldForNode(
      const WebKit::WebNode& node,
      webkit_glue::FormData* form,
      webkit_glue::FormField* field) WARN_UNUSED_RESULT;

  FormManager form_manager_;

  PasswordAutocompleteManager* password_autocomplete_manager_;

  // The ID of the last request sent for form field AutoFill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The node corresponding to the last request sent for form field AutoFill.
  WebKit::WebNode autofill_query_node_;

  // The action to take when receiving AutoFill data from the AutoFillManager.
  AutoFillAction autofill_action_;

  // Should we display a warning if autofill is disabled?
  bool display_warning_if_disabled_;

  // Was the query node autofilled prior to previewing the form?
  bool was_query_node_autofilled_;

  // The menu index of the "Clear" menu item.
  int suggestions_clear_index_;

  // The menu index of the "AutoFill options..." menu item.
  int suggestions_options_index_;

  ScopedRunnableMethodFactory<AutoFillHelper> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillHelper);
};

#endif  // CHROME_RENDERER_AUTOFILL_HELPER_H_
