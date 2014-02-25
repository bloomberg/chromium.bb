// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SOURCE_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace base {
class RefCountedMemory;
}  // namespace base

namespace suggestions {

class SuggestionsProfile;

// SuggestionsSource renders a webpage to list SuggestionsService data.
class SuggestionsSource : public content::URLDataSource {
 public:
  explicit SuggestionsSource(Profile* profile);

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual base::MessageLoop* MessageLoopForRequestPath(
      const std::string& path) const OVERRIDE;
  virtual bool ShouldServiceRequest(
      const net::URLRequest* request) const OVERRIDE;

 private:
  virtual ~SuggestionsSource();

  // Callback passed to a SuggestionsService instance.
  void OnSuggestionsAvailable(
      const content::URLDataSource::GotDataCallback& callback,
      const SuggestionsProfile& suggestions_profile);

  // Only used when servicing requests on the UI thread.
  Profile* const profile_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSource);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SOURCE_H_
