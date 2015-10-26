// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/offline_page_archiver.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
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
//     scoped_ptr<OfflinePageMHTMLArchiver> archiver(
//         new OfflinePageMHTMLArchiver(
//             web_contents, archive_dir));
//     // Callback is of type OfflinePageModel::SavePageCallback.
//     model->SavePage(url, archiver.Pass(), callback);
//   }
class OfflinePageMHTMLArchiver : public OfflinePageArchiver {
 public:
  // Returns the extension name of the offline page file.
  static std::string GetFileNameExtension();
  // Creates a file name for the archive file based on url and title. Public for
  // testing.
  static base::FilePath GenerateFileName(const GURL& url,
                                         const std::string& title);

  OfflinePageMHTMLArchiver(content::WebContents* web_contents,
                           const base::FilePath& archive_dir);
  ~OfflinePageMHTMLArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const CreateArchiveCallback& callback) override;

 protected:
  // Allows to overload the archiver for testing.
  explicit OfflinePageMHTMLArchiver(const base::FilePath& archive_dir);

  // Try to generate MHTML.
  // Might be overridden for testing purpose.
  virtual void GenerateMHTML();

  // Actual call to generate MHTML.
  // Might be overridden for testing purpose.
  virtual void DoGenerateMHTML();

  // Callback for Generating MHTML.
  void OnGenerateMHTMLDone(const GURL& url,
                           const base::FilePath& file_path,
                           int64 file_size);

  // Sends the result of archiving a page to the client that requested archive
  // creation.
  void ReportResult(ArchiverResult result,
                    const GURL& url,
                    const base::FilePath& file_path,
                    int64 file_size);
  void ReportFailure(ArchiverResult result);

 private:
  // Path to the archive directory. It the path is empty, creation of the
  // archive will fail.
  const base::FilePath archive_dir_;
  // Contents of the web page to be serialized. Not owned.
  content::WebContents* web_contents_;

  CreateArchiveCallback callback_;

  base::WeakPtrFactory<OfflinePageMHTMLArchiver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMHTMLArchiver);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
