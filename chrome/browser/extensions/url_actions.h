// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_URL_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_URL_ACTIONS_H_

#include <string>
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/extensions/activity_actions.h"
#include "googleurl/src/gurl.h"

namespace extensions {

// This class describes extension actions that pertain to Content Script
// insertion, Content Script DOM manipulations, and extension XHRs.
class UrlAction : public Action {
 public:
  enum UrlActionType {
    MODIFIED,   // For Content Script DOM manipulations
    READ,       // For Content Script DOM manipulations
    INSERTED,   // For when Content Scripts are added to pages
    XHR,        // When an extension core sends an XHR
  };

  static const char* kTableName;
  static const char* kTableStructure;

  // Create a new UrlAction to describe a ContentScript action
  // or XHR.  All of the parameters should have values except for
  // url_title, which can be an empty string if the ActionType is XHR.
  UrlAction(const std::string& extension_id,
            const base::Time& time,
            const UrlActionType verb,           // what happened
            const GURL& url,                    // the url of the page or XHR
            const string16& url_title,          // the page title
            const std::string& tech_message,    // what goes under "More"
            const std::string& extra);          // any extra logging info

  // Record the action in the database.
  virtual void Record(sql::Connection* db) OVERRIDE;

  // Print a UrlAction with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() OVERRIDE;

  // Print a UrlAction as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() OVERRIDE;

  // Helper methods for retrieving the values.
  const std::string& extension_id() const { return extension_id_; }
  const base::Time& time() const { return time_; }
  std::string VerbAsString() const;
  const GURL& url() const { return url_; }
  const string16& url_title() const { return url_title_; }
  const std::string& technical_message() const { return technical_message_; }
  const std::string& extra() const { return extra_; }

  // Helper methods for restoring a UrlAction from the db.
  static UrlActionType StringAsUrlActionType(const std::string& str);

 protected:
  virtual ~UrlAction();

 private:
  std::string extension_id_;
  base::Time time_;
  UrlActionType verb_;
  GURL url_;
  string16 url_title_;
  std::string technical_message_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(UrlAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_URL_ACTIONS_H_

