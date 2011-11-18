// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_H_
#pragma once

#include <string>
#include <vector>

#include "base/time.h"
#include "base/memory/linked_ptr.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"

// This class is used to represent a notification for an installed app, to be
// displayed on the New Tab Page.
class AppNotification {
 public:
  // Creates an instance with the given properties.
  // If |is_local| is true, notification is not synced.
  // If |guid| is empty, a new guid is automatically created.
  AppNotification(bool is_local,
                  const base::Time& creation_time,
                  const std::string& guid,
                  const std::string& extension_id,
                  const std::string& title,
                  const std::string& body);
  ~AppNotification();

  // Returns a new object that is a copy of this one.
  // Caller owns the returned instance.
  AppNotification* Copy();

  // Setters for optional properties.
  void set_link_url(const GURL& url) { link_url_ = url; }
  void set_link_text(const std::string& text) { link_text_ = text; }
  void set_creation_time(const base::Time& creation_time) {
    creation_time_ = creation_time;
  }

  // Accessors.
  bool is_local() const { return is_local_; }
  const base::Time creation_time() const { return creation_time_; }
  const std::string& guid() const { return guid_; }
  const std::string& extension_id() const { return extension_id_; }
  const std::string& title() const { return title_; }
  const std::string& body() const { return body_; }
  const GURL& link_url() const { return link_url_; }
  const std::string& link_text() const { return link_text_; }

  // Useful methods for serialization.
  void ToDictionaryValue(DictionaryValue* result);
  static AppNotification* FromDictionaryValue(const DictionaryValue& value);

  // Return whether |other| is equal to this object. Useful for tests.
  bool Equals(const AppNotification& other) const;

 private:
  // If you add to the list of data members, make sure to add appropriate checks
  // to the Equals, Copy and {To,From}DictionaryValue methods, keeping in mind
  // backwards compatibility.

  // Whether notification is local only, which means it is not synced
  // across machines.
  bool is_local_;
  base::Time creation_time_;
  std::string guid_;
  std::string extension_id_;
  std::string title_;
  std::string body_;
  GURL link_url_;
  std::string link_text_;

  DISALLOW_COPY_AND_ASSIGN(AppNotification);
};

// A list of AppNotification's.
typedef std::vector<linked_ptr<AppNotification> > AppNotificationList;

// A way to copy an AppNotificationList, which uses AppNotification::Copy on
// each element.
// Caller owns the returned instance.
AppNotificationList* CopyAppNotificationList(const AppNotificationList& source);

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_H_
