// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_URL_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_URL_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/history_url_provider.h"

class AutocompleteProviderClient;
class ClipboardRecentContent;
class HistoryURLProvider;

// Autocomplete provider offering content based on the clipboard's content.
class ClipboardURLProvider : public AutocompleteProvider {
 public:
  ClipboardURLProvider(AutocompleteProviderClient* client,
                       AutocompleteProviderListener* listener,
                       HistoryURLProvider* history_url_provider,
                       ClipboardRecentContent* clipboard_content);

  // AutocompleteProvider implementation.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardURLProviderTest, MatchesImage);

  ~ClipboardURLProvider() override;

  // Handle the match created from one of the match creation methods and do
  // extra tracking and match adding.
  void AddCreatedMatchWithTracking(const AutocompleteInput& input,
                                   const AutocompleteMatch& match);

  // If there is a url copied to the clipboard, use it to create a match.
  base::Optional<AutocompleteMatch> CreateURLMatch(
      const AutocompleteInput& input);
  // If there is text copied to the clipboard, use it to create a match.
  base::Optional<AutocompleteMatch> CreateTextMatch(
      const AutocompleteInput& input);
  // If there is an image copied to the clipboard, use it to create a match.
  // The image match is asynchronous (because constructing the image post data
  // takes time), so instead of returning an optional match like the other
  // Create functions, it returns a boolean indicating whether there will be a
  // match.
  bool CreateImageMatch(const AutocompleteInput& input);

  // Resize and encode the image data into bytes. This can take some time if the
  // image is large, so this should happen on a background thread.
  static scoped_refptr<base::RefCountedMemory> EncodeClipboardImage(
      gfx::Image image);
  // Construct the actual image match once the image has been encoded into
  // bytes. This should be called back on the main thread.
  void ConstructImageMatchCallback(
      const AutocompleteInput& input,
      TemplateURLService* url_service,
      scoped_refptr<base::RefCountedMemory> image_bytes);

  AutocompleteProviderClient* client_;
  AutocompleteProviderListener* listener_;
  ClipboardRecentContent* clipboard_content_;

  // Used for efficiency when creating the verbatim match.  Can be NULL.
  HistoryURLProvider* history_url_provider_;

  // The current URL suggested and the number of times it has been offered.
  // Used for recording metrics.
  GURL current_url_suggested_;
  size_t current_url_suggested_times_;

  // Used to cancel image construction callbacks if autocomplete Stop() is
  // called.
  base::WeakPtrFactory<ClipboardURLProvider> callback_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardURLProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_URL_PROVIDER_H_
