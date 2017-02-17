// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_SHELF_ID_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_SHELF_ID_H_

#include <string>

// Represents ARC app shelf id that consists from app id and optional shelf
// group id.
namespace arc {

class ArcAppShelfId {
 public:
  ArcAppShelfId(const std::string& shelf_group_id, const std::string& app_id);
  ArcAppShelfId(const ArcAppShelfId& other);
  ~ArcAppShelfId();

  // Returns id from string representation which has syntax
  // "shelf_group:some_group_id:some_app_id". In case suffix shelf_group is
  // absent then id is treated as app id.
  static ArcAppShelfId FromString(const std::string& id);

  // Constructs id from app id and optional shelf_group_id encoded into the
  // |intent|.
  static ArcAppShelfId FromIntentAndAppId(const std::string& intent,
                                          const std::string& app_id);

  // Returns string representation of this id.
  std::string ToString() const;

  bool operator<(const ArcAppShelfId& other) const;

  bool has_shelf_group_id() const { return !shelf_group_id_.empty(); }

  const std::string& shelf_group_id() const { return shelf_group_id_; }

  const std::string& app_id() const { return app_id_; }

 private:
  const std::string shelf_group_id_;
  const std::string app_id_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_SHELF_ID_H_
