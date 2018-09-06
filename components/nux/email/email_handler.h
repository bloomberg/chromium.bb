// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NUX_EMAIL_EMAIL_HANDLER_H_
#define COMPONENTS_NUX_EMAIL_EMAIL_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrefService;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace content {
class WebUIDataSource;
}  // namespace content

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace nux {

extern const char* kEmailInteractionHistogram;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailInteraction {
  kPromptShown = 0,
  kNoThanks = 1,
  kGetStarted = 2,
  kCount,
};

class EmailHandler : public content::WebUIMessageHandler {
 public:
  EmailHandler(PrefService* prefs,
               favicon::FaviconService* favicon_service,
               bookmarks::BookmarkModel* bookmark_model);
  ~EmailHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleRejectEmails(const base::ListValue* args);
  void HandleAddEmails(const base::ListValue* args);
  void HandleToggleBookmarkBar(const base::ListValue* args);

  // Adds webui sources.
  static void AddSources(content::WebUIDataSource* html_source,
                         PrefService* prefs);

 private:
  // Weak reference.
  PrefService* prefs_;

  // Weak reference.
  favicon::FaviconService* favicon_service_;

  // Weak reference.
  bookmarks::BookmarkModel* bookmark_model_;

  DISALLOW_COPY_AND_ASSIGN(EmailHandler);
};

}  // namespace nux

#endif  // COMPONENTS_NUX_GOOGLE_APPS_GOOGLE_APPS_HANDLER_H_
