// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "googleurl/src/gurl.h"

// This class is used to represent a notification for an installed app, to be
// displayed on the New Tab Page.
class AppNotification {
 public:
  AppNotification(const std::string& title, const std::string& body);
  ~AppNotification();

  // Setters for optional properties.
  void set_link_url(const GURL& url) { link_url_ = url; }
  void set_link_text(const std::string& text) { link_text_ = text; }

  // Accessors.
  const std::string& title() const { return title_; }
  const std::string& body() const { return body_; }
  const GURL& link_url() const { return link_url_; }
  const std::string& link_text() const { return link_text_; }

 private:
  std::string title_;
  std::string body_;
  GURL link_url_;
  std::string link_text_;

  DISALLOW_COPY_AND_ASSIGN(AppNotification);
};

// A list of AppNotification's.
typedef std::vector<linked_ptr<AppNotification> > AppNotificationList;

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_H_
