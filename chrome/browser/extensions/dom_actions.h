// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DOM_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_DOM_ACTIONS_H_

#include "base/string16.h"
#include "chrome/browser/extensions/activity_actions.h"
#include "googleurl/src/gurl.h"

namespace extensions {

// This class describes extension actions that pertain to DOM API calls and
// content script insertions.
class DOMAction : public Action {
 public:
  enum DOMActionType {
    MODIFIED,   // For Content Script DOM manipulations
    READ,       // For Content Script DOM manipulations
    INSERTED,   // For when Content Scripts are added to pages
    XHR,        // When an extension core sends an XHR
  };

  static const char* kTableName;
  static const char* kTableContentFields[];

  // Create a new database table for storing DOMActions, or update the schema if
  // it is out of date. Any existing data is preserved.
  static bool InitializeTable(sql::Connection* db);

  // Create a new DOMAction to describe a new DOM API call.
  // If the DOMAction is on a background page, the url & url_title may be null.
  // If the DOMAction refers to a content script insertion, api_call may be null
  // but args should be the name of the content script.
  DOMAction(const std::string& extension_id,
            const base::Time& time,
            const DOMActionType verb,           // what happened
            const GURL& url,                    // the url of the page the
                                                // script is running on
            const string16& url_title,          // the page title
            const std::string& api_call,        // the DOM API call
            const std::string& args,            // the args
            const std::string& extra);          // any extra logging info

  // Create a new DOMAction from a database row.
  explicit DOMAction(const sql::Statement& s);

  // Record the action in the database.
  virtual void Record(sql::Connection* db) OVERRIDE;

  // Print a DOMAction with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() OVERRIDE;

  // Print a DOMAction as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() OVERRIDE;

  // Helper methods for retrieving the values.
  std::string VerbAsString() const;
  const GURL& url() const { return url_; }
  const string16& url_title() const { return url_title_; }
  const std::string& api_call() const { return api_call_; }
  const std::string& args() const { return args_; }
  const std::string& extra() const { return extra_; }

  // Helper methods for restoring a DOMAction from the db.
  static DOMActionType StringAsDOMActionType(const std::string& str);

 protected:
  virtual ~DOMAction();

 private:
  DOMActionType verb_;
  GURL url_;
  string16 url_title_;
  std::string api_call_;
  std::string args_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(DOMAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DOM_ACTIONS_H_

