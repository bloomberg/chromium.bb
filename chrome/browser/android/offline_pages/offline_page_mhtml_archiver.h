// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_archiver.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace offline_pages {

// Class implementing an offline page archiver using MHTML as an archive format.
//
// To generate an MHTML archiver for a given URL, a WebContents instance should
// have that URL loaded.
//
// Example:
//   void SavePageOffline(content::WebContents* web_contents) {
//     const GURL& url = web_contents->GetLastCommittedURL();
//     std::unique_ptr<OfflinePageMHTMLArchiver> archiver(
//         new OfflinePageMHTMLArchiver(
//             web_contents, archive_dir));
//     // Callback is of type OfflinePageModel::SavePageCallback.
//     model->SavePage(url, std::move(archiver), callback);
//   }
class OfflinePageMHTMLArchiver : public OfflinePageArchiver {
 public:

  explicit OfflinePageMHTMLArchiver(content::WebContents* web_contents);
  ~OfflinePageMHTMLArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params,
                     const CreateArchiveCallback& callback) override;

 protected:
  // Allows to overload the archiver for testing.
  OfflinePageMHTMLArchiver();

  // Try to generate MHTML.
  // Might be overridden for testing purpose.
  virtual void GenerateMHTML(const base::FilePath& archives_dir,
                             const CreateArchiveParams& create_archive_params);

  // Callback for Generating MHTML.
  void OnGenerateMHTMLDone(const GURL& url,
                           const base::FilePath& file_path,
                           const base::string16& title,
                           int64_t file_size);

  // Checks whether the page to be saved has security error when loaded over
  // HTTPS. Saving a page will fail if that is the case. HTTP connections are
  // not affected.
  virtual bool HasConnectionSecurityError();

  // Reports failure to create archive a page to the client that requested it.
  void ReportFailure(ArchiverResult result);

 private:
  // Contents of the web page to be serialized. Not owned.
  content::WebContents* web_contents_;

  CreateArchiveCallback callback_;

  base::WeakPtrFactory<OfflinePageMHTMLArchiver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMHTMLArchiver);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
