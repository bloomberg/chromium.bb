// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTION_SOURCE_H_
#define CHROME_BROWSER_SEARCH_SUGGESTION_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

// Serves HTML for displaying suggestions using iframes, e.g.
// chrome-search://suggestion/loader.html
class SuggestionSource : public content::URLDataSource {
 public:
  SuggestionSource();
  virtual ~SuggestionSource();

 protected:
  virtual bool GetOrigin(
      int process_id,
      int render_view_id,
      std::string* origin) const;

  // Overridden from content::URLDataSource:
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path_and_query,
      int render_process_id,
      int render_view_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(
      const std::string& path_and_query) const OVERRIDE;
  virtual bool ShouldDenyXFrameOptions() const OVERRIDE;
  virtual bool ShouldServiceRequest(
      const net::URLRequest* request) const OVERRIDE;

 private:
  // Sends unmodified resource bytes.
  void SendResource(
      int resource_id,
      const content::URLDataSource::GotDataCallback& callback);

  // Sends Javascript with an expected postMessage origin interpolated.
  void SendJSWithOrigin(
      int resource_id,
      int render_process_id,
      int render_view_id,
      const content::URLDataSource::GotDataCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(SuggestionSource);
};

#endif  // CHROME_BROWSER_SEARCH_SUGGESTION_SOURCE_H_
