// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_FIND_HELPER_H_
#define CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_FIND_HELPER_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
class WebviewFindFunction;
} // namespace extensions

// Helper class for find requests and replies for the webview find API.
class WebviewFindHelper {
 public:
  WebviewFindHelper();
  ~WebviewFindHelper();

  // Cancels all find requests in progress and calls their callback functions.
  void CancelAllFindSessions();

  // Ends the find session with id |session_request_id|  and calls the
  // appropriate callbacks.
  void EndFindSession(int session_request_id, bool canceled);

  // Helper function for WebViewGuest::Find().
  void Find(content::WebContents* guest_web_contents,
            const base::string16& search_text,
            const blink::WebFindOptions& options,
            scoped_refptr<extensions::WebviewFindFunction> find_function);

  // Helper function for WeViewGuest:FindReply().
  void FindReply(int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update);

 private:
  // A wrapper to store find results.
  class FindResults {
   public:
    FindResults();
    ~FindResults();

    // Aggregate the find results.
    void AggregateResults(int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update);

    // Stores find results into a DictionaryValue.
    void PrepareResults(base::DictionaryValue* results);

   private:
    int number_of_matches_;
    int active_match_ordinal_;
    gfx::Rect selection_rect_;

    friend void WebviewFindHelper::EndFindSession(int session_request_id,
                                                  bool canceled);

    DISALLOW_COPY_AND_ASSIGN(FindResults);
  };

  // Handles all information about a find request and its results.
  class FindInfo {
   public:
    FindInfo(int request_id,
             const base::string16& search_text,
             const blink::WebFindOptions& options,
             scoped_refptr<extensions::WebviewFindFunction> find_function);
    ~FindInfo();

    // Add another request to |find_next_requests_|.
    void AddFindNextRequest(const base::WeakPtr<FindInfo>& request) {
      find_next_requests_.push_back(request);
    }

    // Aggregate the find results.
    void AggregateResults(int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update);

    base::WeakPtr<FindInfo> AsWeakPtr();

    blink::WebFindOptions* options() {
      return &options_;
    }

    int request_id() {
      return request_id_;
    }

    const base::string16& search_text() {
      return search_text_;
    }

    // Calls the callback function within |find_function_| with the find results
    // from within |find_results_|.
    void SendResponse(bool canceled);

   private:
    const int request_id_;
    const base::string16 search_text_;
    blink::WebFindOptions options_;
    scoped_refptr<extensions::WebviewFindFunction> find_function_;
    FindResults find_results_;

    // A find reply has been received for this find request.
    bool replied_;

    // Stores pointers to all the find next requests if this is the first
    // request of a find session.
    std::vector<base::WeakPtr<FindInfo> > find_next_requests_;

    // Weak pointer used to access the find info of fin.
    base::WeakPtrFactory<FindInfo> weak_ptr_factory_;

    friend void WebviewFindHelper::EndFindSession(int session_request_id,
                                                  bool canceled);

    DISALLOW_COPY_AND_ASSIGN(FindInfo);
  };

  // A counter to generate a unique request id for a find request.
  // We only need the ids to be unique for a given WebViewGuest.
  int current_find_request_id_;

  // Pointer to the first request of the current find session.
  linked_ptr<FindInfo> current_find_session_;

  // Stores each find request's information by request_id so that its callback
  // function can be called when its find results are available.
  typedef std::map<int, linked_ptr<FindInfo> > FindInfoMap;
  FindInfoMap find_info_map_;

  DISALLOW_COPY_AND_ASSIGN(WebviewFindHelper);
};

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_FIND_HELPER_H_
