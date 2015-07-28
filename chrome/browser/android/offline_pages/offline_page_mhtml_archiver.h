// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_

#include <map>

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
//             web_contents, archiver_dir, task_runner));
//     // Callback is of type OfflinePageModel::SavePageCallback.
//     model->SavePage(url, archiver.Pass(), callback);
//   }
class OfflinePageMHTMLArchiver : public OfflinePageArchiver {
 public:
  OfflinePageMHTMLArchiver(
      content::WebContents* web_contents,
      const base::FilePath& file_path,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~OfflinePageMHTMLArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const CreateArchiveCallback& callback) override;

 protected:
  // Allows to overload the archiver for testing.
  OfflinePageMHTMLArchiver(
      const base::FilePath& file_path,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Try to generate MHTML.
  // Might be overridden for testing purpose.
  virtual void GenerateMHTML();

  // Actual call to generate MHTML.
  // Might be overridden for testing purpose.
  virtual void DoGenerateMHTML();

  // Callback for Generating MHTML.
  void OnGenerateMHTMLDone(const GURL& url,
                           const base::string16& title,
                           int64 file_size);

  // Sends the result of archiving a page to the client that requested archive
  // creation.
  void ReportResult(ArchiverResult result,
                    const GURL& url,
                    const base::string16& title,
                    int64 file_size);
  void ReportFailure(ArchiverResult result);

 private:
  // Path to the archive file.
  const base::FilePath file_path_;
  // Contents of the web page to be serialized. Not owned.
  // TODO(fgorski): Add WebContentsObserver to know when the page navigates away
  // or shuts down.
  content::WebContents* web_contents_;

  CreateArchiveCallback callback_;

  // Task runner, which will be used to post the callback.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<OfflinePageMHTMLArchiver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMHTMLArchiver);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_MHTML_ARCHIVER_H_
