// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/gtk_dnd_util.h"
#include "base/pickle.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(GtkDndUtilTest, ExtractNamedURLValid) {
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.foobar.com/";
  Pickle pickle;
  pickle.WriteString(kTitle);
  pickle.WriteString(kUrl);

  GtkSelectionData data;
  scoped_array<guchar> test_data(new guchar[pickle.size()]);
  memcpy(test_data.get(), pickle.data(), pickle.size());
  data.data = test_data.get();
  data.length = pickle.size();

  GURL url;
  string16 title;
  ASSERT_EQ(true, gtk_dnd_util::ExtractNamedURL(&data, &url, &title));
  EXPECT_EQ(UTF8ToUTF16(kTitle), title);
  EXPECT_EQ(GURL(kUrl), url);
}

TEST(GtkDndUtilTest, ExtractNamedURLInvalidURL) {
  const std::string kTitle = "title";
  const std::string kBadUrl = "foobar";
  Pickle pickle;
  pickle.WriteString(kTitle);
  pickle.WriteString(kBadUrl);

  GtkSelectionData data;
  scoped_array<guchar> test_data(new guchar[pickle.size()]);
  memcpy(test_data.get(), pickle.data(), pickle.size());
  data.data = test_data.get();
  data.length = pickle.size();

  GURL url;
  string16 title;
  EXPECT_EQ(false, gtk_dnd_util::ExtractNamedURL(&data, &url, &title));
}

TEST(GtkDndUtilTest, ExtractNamedURLInvalidInput) {
  GURL url;
  string16 title;
  GtkSelectionData data;
  data.data = NULL;
  data.length = 0;

  EXPECT_EQ(false, gtk_dnd_util::ExtractNamedURL(&data, &url, &title));

  guchar empty_data[] = "";
  data.data = empty_data;
  data.length = 0;

  EXPECT_EQ(false, gtk_dnd_util::ExtractNamedURL(&data, &url, &title));

  const std::string kTitle = "title";
  Pickle pickle;
  pickle.WriteString(kTitle);

  scoped_array<guchar> test_data(new guchar[pickle.size()]);
  memcpy(test_data.get(), pickle.data(), pickle.size());
  data.data = test_data.get();
  data.length = pickle.size();

  EXPECT_EQ(false, gtk_dnd_util::ExtractNamedURL(&data, &url, &title));
}
