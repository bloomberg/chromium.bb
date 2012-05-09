// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_H_
#pragma once

class Profile;
class SuggestionsCombiner;

namespace base {
class DictionaryValue;
}

// Interface for a source of suggested pages. The various sources will be
// combined by the SuggestionsCombiner.
class SuggestionsSource {

 public:
  virtual ~SuggestionsSource() {}

 protected:
  SuggestionsSource() {}

  friend class SuggestionsCombiner;

  // The source's weight indicates how many items from this source will be
  // selected by the combiner. If a source weight is x and the total weight of
  // all the sources is y, then the combiner will select x/y items from it.
  virtual int GetWeight() = 0;

  // Returns the number of items this source can produce.
  virtual int GetItemCount() = 0;

  // Removes the most important item from this source and returns it. The
  // returned item must be in the right format to be sent to the javascript,
  // which you typically get by calling NewTabUI::SetURLTitleAndDirection. If
  // the source is empty this method returns null.
  // The caller takes ownership of the returned item.
  virtual base::DictionaryValue* PopItem() = 0;

  // Requests that the source fetch its items. If the source is already fetching
  // it does not have to reset and can continue fetching.
  virtual void FetchItems(Profile* profile) = 0;

  // Sets the combiner holding this suggestion source. This method can only
  // be called once.
  virtual void SetCombiner(SuggestionsCombiner* combiner) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestionsSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_H_
