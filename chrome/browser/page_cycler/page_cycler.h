// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CYCLER_PAGE_CYCLER_H_
#define CHROME_BROWSER_PAGE_CYCLER_PAGE_CYCLER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;

namespace content {
class RenderViewHost;
}  // namespace content

namespace base {
class TimeTicks;
}  // namespace base

// Performance test to track the resources used and speed with which chromium
// fully loads a given set of URLs.  This class is created on the UI thread and
// does most of its work there. However, some work happens on background threads
// too; those are named with 'OnBackgroundThread'.
class PageCycler : public base::RefCountedThreadSafe<PageCycler>,
                   public chrome::BrowserListObserver,
                   public content::WebContentsObserver {
 public:
  PageCycler(Browser* browser, const base::FilePath& urls_file);

  // Begin running the page cycler.
  void Run();

  // content::WebContentsObserver
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;

  // This method should never be necessary while running PageCycler; this is
  // for testing purposes only.
  const std::vector<GURL>* urls_for_test() { return &urls_; }

  void set_stats_file(const base::FilePath& stats_file) {
    stats_file_ = stats_file;
  }
  void set_errors_file(const base::FilePath& errors_file) {
    errors_file_ = errors_file;
  }


 protected:
  virtual ~PageCycler();

 private:
  friend class base::RefCountedThreadSafe<PageCycler>;
  friend class MockPageCycler;

  // Check to see if a load callback is valid; i.e. the load should be from the
  // main frame, the url should not be a chrome error url, and |url_index|
  // should not be 0.
  bool IsLoadCallbackValid(const GURL& validated_url,
                           bool is_main_frame);

  // Read in the urls from |urls_file_| and store them in |urls_|.
  void ReadURLsOnBackgroundThread();

  // Perform any initial setup neccessary, and begin visiting the pages.
  void BeginCycle();

  // If |url_index_| points to a valid position in |urls_|, load the url,
  // capturing any statistics information. Otherwise, call WriteResults.
  void LoadNextURL();

  // Complete statistics gathering for the finished visit, and try to load the
  // next url.
  void LoadSucceeded();

  // Inidicate that the load failed with an error; try to load the next url.
  void LoadFailed(const GURL& url, const string16& error_description);

  // Finalize the output strings.
  void PrepareResultsOnBackgroundThread();

  // Write the data stored within |output| to the file indicated by
  // |stats_file_|, if |stats_file_| is not empty. Write any errors to
  // |errors_file_|.
  void WriteResultsOnBackgroundThread(const std::string& output);

  // Perform any necessary cleanup and exit |browser_|; virtual since tests may
  // need to override this function.
  virtual void Finish();

  // Called when the Browser to which |browser_| points is closed; exits
  // PageCycler.
  void Abort();

  // chrome::BrowserListObserver
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // The Browser context in which the page cycler is running.
  Browser* browser_;

  // The path to the file containing the list of urls to visit.
  base::FilePath urls_file_;

  // The path to the file to which we write any errors encountered.
  base::FilePath errors_file_;

  // The path to the file to which we write the statistics (optional, may be
  // an empty path).
  base::FilePath stats_file_;

  // The list of urls to visit.
  std::vector<GURL> urls_;

  // The current index into the |urls_| vector.
  size_t url_index_;

  // The generated string of urls which we have visited; this is built one url
  // at a time as we iterate through the |urls_| vector. This is primarily
  // included for interfacing with the previous page_cycler's output style.
  std::string urls_string_;

  // The generated string of the times taken to visit each url. As with
  // |urls_string_|, this is built as we visit each url, and is primarily to
  // produce output similar to the previous page_cycler's.
  std::string timings_string_;

  // The time at which we begin the process of loading the next url; this is
  // used to calculate the time taken for each url load.
  base::TimeTicks initial_time_;

  // Indicates the abort status of the page cycler; true means aborted.
  bool aborted_;

  string16 error_;

  DISALLOW_COPY_AND_ASSIGN(PageCycler);
};

#endif  // CHROME_BROWSER_PAGE_CYCLER_PAGE_CYCLER_H_
