// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/omnibox/omnibox_popup_view_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "testing/platform_test.h"

namespace {

const float kLargeWidth = 10000;

const GdkColor kContentTextColor = GDK_COLOR_RGB(0x00, 0x00, 0x00);
const GdkColor kDimContentTextColor = GDK_COLOR_RGB(0x80, 0x80, 0x80);
const GdkColor kURLTextColor = GDK_COLOR_RGB(0x00, 0x88, 0x00);

}  // namespace

class OmniboxPopupViewGtkTest : public PlatformTest {
 public:
  OmniboxPopupViewGtkTest() {}

  virtual void SetUp() {
    PlatformTest::SetUp();

    window_ = gtk_window_new(GTK_WINDOW_POPUP);
    layout_ = gtk_widget_create_pango_layout(window_, NULL);
  }

  virtual void TearDown() {
    g_object_unref(layout_);
    gtk_widget_destroy(window_);

    PlatformTest::TearDown();
  }

  // The google C++ Testing Framework documentation suggests making
  // accessors in the fixture so that each test doesn't need to be a
  // friend of the class being tested.  This method just proxies the
  // call through after adding the fixture's layout_.
  void SetupLayoutForMatch(
      const string16& text,
      const AutocompleteMatch::ACMatchClassifications& classifications,
      const GdkColor* base_color,
      const GdkColor* dim_color,
      const GdkColor* url_color,
      const std::string& prefix_text) {
    OmniboxPopupViewGtk::SetupLayoutForMatch(layout_,
                                             text,
                                             classifications,
                                             base_color,
                                             dim_color,
                                             url_color,
                                             prefix_text);
  }

  struct RunInfo {
    PangoAttribute* attr_;
    guint length_;
    RunInfo() : attr_(NULL), length_(0) { }
  };

  RunInfo RunInfoForAttrType(guint location,
                             guint end_location,
                             PangoAttrType type) {
    RunInfo retval;

    PangoAttrList* attrs = pango_layout_get_attributes(layout_);
    if (!attrs)
      return retval;

    PangoAttrIterator* attr_iter = pango_attr_list_get_iterator(attrs);
    if (!attr_iter)
      return retval;

    for (gboolean more = true, findNextStart = false;
        more;
        more = pango_attr_iterator_next(attr_iter)) {
      PangoAttribute* attr = pango_attr_iterator_get(attr_iter, type);

      // This iterator segment doesn't have any elements of the
      // desired type; keep looking.
      if (!attr)
        continue;

      // Skip attribute ranges before the desired start point.
      if (attr->end_index <= location)
        continue;

      // If the matching type went past the iterator segment, then set
      // the length to the next start - location.
      if (findNextStart) {
        // If the start is still less than the location, then reset
        // the match.  Otherwise, check that the new attribute is, in
        // fact different before shortening the run length.
        if (attr->start_index <= location) {
          findNextStart = false;
        } else if (!pango_attribute_equal(retval.attr_, attr)) {
          retval.length_ = attr->start_index - location;
          break;
        }
      }

      gint start_range, end_range;
      pango_attr_iterator_range(attr_iter,
                                &start_range,
                                &end_range);

      // Now we have a match.  May need to keep going to shorten
      // length if we reach a new item of the same type.
      retval.attr_ = attr;
      if (attr->end_index > (guint)end_range) {
        retval.length_ = end_location - location;
        findNextStart = true;
      } else {
        retval.length_ = attr->end_index - location;
        break;
      }
    }

    pango_attr_iterator_destroy(attr_iter);
    return retval;
  }

  guint RunLengthForAttrType(guint location,
                             guint end_location,
                             PangoAttrType type) {
    RunInfo info = RunInfoForAttrType(location,
                                      end_location,
                                      type);
    return info.length_;
  }

  gboolean RunHasAttribute(guint location,
                           guint end_location,
                           PangoAttribute* attribute) {
    RunInfo info = RunInfoForAttrType(location,
                                      end_location,
                                      attribute->klass->type);

    return info.attr_ && pango_attribute_equal(info.attr_, attribute);
  }

  gboolean RunHasColor(guint location,
                       guint end_location,
                       const GdkColor& color) {
    PangoAttribute* attribute =
        pango_attr_foreground_new(color.red,
                                  color.green,
                                  color.blue);

    gboolean retval = RunHasAttribute(location,
                                      end_location,
                                      attribute);

    pango_attribute_destroy(attribute);

    return retval;
  }

  gboolean RunHasWeight(guint location,
                        guint end_location,
                        PangoWeight weight) {
    PangoAttribute* attribute = pango_attr_weight_new(weight);

    gboolean retval = RunHasAttribute(location,
                                      end_location,
                                      attribute);

    pango_attribute_destroy(attribute);

    return retval;
  }

  GtkWidget* window_;
  PangoLayout* layout_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupViewGtkTest);
};

// Simple inputs with no matches should result in styled output who's
// text matches the input string, with the passed-in color, and
// nothing bolded.
TEST_F(OmniboxPopupViewGtkTest, DecorateMatchedStringNoMatch) {
  const string16 kContents = ASCIIToUTF16("This is a test");

  AutocompleteMatch::ACMatchClassifications classifications;

  SetupLayoutForMatch(kContents,
                      classifications,
                      &kContentTextColor,
                      &kDimContentTextColor,
                      &kURLTextColor,
                      std::string());

  EXPECT_EQ(kContents.length(), RunLengthForAttrType(0U, kContents.length(),
                                                     PANGO_ATTR_FOREGROUND));

  EXPECT_TRUE(RunHasColor(0U, kContents.length(), kContentTextColor));

  // This part's a little wacky - either we don't have a weight, or
  // the weight run is the entire string and is NORMAL
  guint weightLength = RunLengthForAttrType(0U, kContents.length(),
                                           PANGO_ATTR_WEIGHT);
  if (weightLength) {
    EXPECT_EQ(kContents.length(), weightLength);
    EXPECT_TRUE(RunHasWeight(0U, kContents.length(), PANGO_WEIGHT_NORMAL));
  }
}

// Identical to DecorateMatchedStringNoMatch, except test that URL
// style gets a different color than we passed in.
TEST_F(OmniboxPopupViewGtkTest, DecorateMatchedStringURLNoMatch) {
  const string16 kContents = ASCIIToUTF16("This is a test");
  AutocompleteMatch::ACMatchClassifications classifications;

  classifications.push_back(
      ACMatchClassification(0U, ACMatchClassification::URL));

  SetupLayoutForMatch(kContents,
                      classifications,
                      &kContentTextColor,
                      &kDimContentTextColor,
                      &kURLTextColor,
                      std::string());

  EXPECT_EQ(kContents.length(), RunLengthForAttrType(0U, kContents.length(),
                                                     PANGO_ATTR_FOREGROUND));
  EXPECT_TRUE(RunHasColor(0U, kContents.length(), kURLTextColor));

  // This part's a little wacky - either we don't have a weight, or
  // the weight run is the entire string and is NORMAL
  guint weightLength = RunLengthForAttrType(0U, kContents.length(),
                                           PANGO_ATTR_WEIGHT);
  if (weightLength) {
    EXPECT_EQ(kContents.length(), weightLength);
    EXPECT_TRUE(RunHasWeight(0U, kContents.length(), PANGO_WEIGHT_NORMAL));
  }
}

// Test that DIM works as expected.
TEST_F(OmniboxPopupViewGtkTest, DecorateMatchedStringDimNoMatch) {
  const string16 kContents = ASCIIToUTF16("This is a test");
  // Dim "is".
  const guint kRunLength1 = 5, kRunLength2 = 2, kRunLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(kRunLength1 + kRunLength2 + kRunLength3, kContents.length());

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0U, ACMatchClassification::NONE));
  classifications.push_back(
      ACMatchClassification(kRunLength1, ACMatchClassification::DIM));
  classifications.push_back(
      ACMatchClassification(kRunLength1 + kRunLength2,
                            ACMatchClassification::NONE));

  SetupLayoutForMatch(kContents,
                      classifications,
                      &kContentTextColor,
                      &kDimContentTextColor,
                      &kURLTextColor,
                      std::string());

  // Check the runs have expected color and length.
  EXPECT_EQ(kRunLength1, RunLengthForAttrType(0U, kContents.length(),
                                              PANGO_ATTR_FOREGROUND));
  EXPECT_TRUE(RunHasColor(0U, kContents.length(), kContentTextColor));
  EXPECT_EQ(kRunLength2, RunLengthForAttrType(kRunLength1, kContents.length(),
                                              PANGO_ATTR_FOREGROUND));
  EXPECT_TRUE(RunHasColor(kRunLength1, kContents.length(),
                          kDimContentTextColor));
  EXPECT_EQ(kRunLength3, RunLengthForAttrType(kRunLength1 + kRunLength2,
                                              kContents.length(),
                                              PANGO_ATTR_FOREGROUND));
  EXPECT_TRUE(RunHasColor(kRunLength1 + kRunLength2, kContents.length(),
                          kContentTextColor));

  // This part's a little wacky - either we don't have a weight, or
  // the weight run is the entire string and is NORMAL
  guint weightLength = RunLengthForAttrType(0U, kContents.length(),
                                            PANGO_ATTR_WEIGHT);
  if (weightLength) {
    EXPECT_EQ(kContents.length(), weightLength);
    EXPECT_TRUE(RunHasWeight(0U, kContents.length(), PANGO_WEIGHT_NORMAL));
  }
}

// Test that the matched run gets bold-faced, but keeps the same
// color.
TEST_F(OmniboxPopupViewGtkTest, DecorateMatchedStringMatch) {
  const string16 kContents = ASCIIToUTF16("This is a test");
  // Match "is".
  const guint kRunLength1 = 5, kRunLength2 = 2, kRunLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(kRunLength1 + kRunLength2 + kRunLength3, kContents.length());

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0U, ACMatchClassification::NONE));
  classifications.push_back(
      ACMatchClassification(kRunLength1, ACMatchClassification::MATCH));
  classifications.push_back(
      ACMatchClassification(kRunLength1 + kRunLength2,
                            ACMatchClassification::NONE));

  SetupLayoutForMatch(kContents,
                      classifications,
                      &kContentTextColor,
                      &kDimContentTextColor,
                      &kURLTextColor,
                      std::string());

  // Check the runs have expected weight and length.
  EXPECT_EQ(kRunLength1, RunLengthForAttrType(0U, kContents.length(),
                                              PANGO_ATTR_WEIGHT));
  EXPECT_TRUE(RunHasWeight(0U, kContents.length(), PANGO_WEIGHT_NORMAL));
  EXPECT_EQ(kRunLength2, RunLengthForAttrType(kRunLength1, kContents.length(),
                                              PANGO_ATTR_WEIGHT));
  EXPECT_TRUE(RunHasWeight(kRunLength1, kContents.length(), PANGO_WEIGHT_BOLD));
  EXPECT_EQ(kRunLength3, RunLengthForAttrType(kRunLength1 + kRunLength2,
                                              kContents.length(),
                                              PANGO_ATTR_WEIGHT));
  EXPECT_TRUE(RunHasWeight(kRunLength1 + kRunLength2, kContents.length(),
                           PANGO_WEIGHT_NORMAL));

  // The entire string should be the same, normal color.
  EXPECT_EQ(kContents.length(), RunLengthForAttrType(0U, kContents.length(),
                                                     PANGO_ATTR_FOREGROUND));
  EXPECT_TRUE(RunHasColor(0U, kContents.length(), kContentTextColor));
}

// Just like DecorateMatchedStringURLMatch, this time with URL style.
TEST_F(OmniboxPopupViewGtkTest, DecorateMatchedStringURLMatch) {
  const string16 kContents = ASCIIToUTF16("http://hello.world/");
  // Match "hello".
  const guint kRunLength1 = 7, kRunLength2 = 5, kRunLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(kRunLength1 + kRunLength2 + kRunLength3, kContents.length());

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0U, ACMatchClassification::URL));
  const int kURLMatch =
      ACMatchClassification::URL | ACMatchClassification::MATCH;
  classifications.push_back(
      ACMatchClassification(kRunLength1, kURLMatch));
  classifications.push_back(
      ACMatchClassification(kRunLength1 + kRunLength2,
                            ACMatchClassification::URL));

  SetupLayoutForMatch(kContents,
                      classifications,
                      &kContentTextColor,
                      &kDimContentTextColor,
                      &kURLTextColor,
                      std::string());

  // One color for the entire string, and it's not the one we passed
  // in.
  EXPECT_EQ(kContents.length(), RunLengthForAttrType(0U, kContents.length(),
                                                     PANGO_ATTR_FOREGROUND));
  EXPECT_TRUE(RunHasColor(0U, kContents.length(), kURLTextColor));
}
