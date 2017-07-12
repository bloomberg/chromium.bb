// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace default_user_image {

// Resource IDs of default user images.
const int kDefaultImageResourceIDs[] = {
    IDR_LOGIN_DEFAULT_USER,
    IDR_LOGIN_DEFAULT_USER_1,
    IDR_LOGIN_DEFAULT_USER_2,
    IDR_LOGIN_DEFAULT_USER_3,
    IDR_LOGIN_DEFAULT_USER_4,
    IDR_LOGIN_DEFAULT_USER_5,
    IDR_LOGIN_DEFAULT_USER_6,
    IDR_LOGIN_DEFAULT_USER_7,
    IDR_LOGIN_DEFAULT_USER_8,
    IDR_LOGIN_DEFAULT_USER_9,
    IDR_LOGIN_DEFAULT_USER_10,
    IDR_LOGIN_DEFAULT_USER_11,
    IDR_LOGIN_DEFAULT_USER_12,
    IDR_LOGIN_DEFAULT_USER_13,
    IDR_LOGIN_DEFAULT_USER_14,
    IDR_LOGIN_DEFAULT_USER_15,
    IDR_LOGIN_DEFAULT_USER_16,
    IDR_LOGIN_DEFAULT_USER_17,
    IDR_LOGIN_DEFAULT_USER_18,
    IDR_LOGIN_DEFAULT_USER_19,
    IDR_LOGIN_DEFAULT_USER_20,
    IDR_LOGIN_DEFAULT_USER_21,
    IDR_LOGIN_DEFAULT_USER_22,
    IDR_LOGIN_DEFAULT_USER_23,
    IDR_LOGIN_DEFAULT_USER_24,
    IDR_LOGIN_DEFAULT_USER_25,
    IDR_LOGIN_DEFAULT_USER_26,
    IDR_LOGIN_DEFAULT_USER_27,
    IDR_LOGIN_DEFAULT_USER_28,
    IDR_LOGIN_DEFAULT_USER_29,
    IDR_LOGIN_DEFAULT_USER_30,
    IDR_LOGIN_DEFAULT_USER_31,
    IDR_LOGIN_DEFAULT_USER_32,
    IDR_LOGIN_DEFAULT_USER_33,
};

const int kDefaultImageAuthorIDs[] = {
    IDS_LOGIN_DEFAULT_USER_AUTHOR,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_1,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_2,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_3,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_4,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_5,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_6,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_7,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_8,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_9,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_10,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_11,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_12,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_13,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_14,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_15,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_16,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_17,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_18,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_19,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_20,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_21,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_22,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_23,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_24,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_25,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_26,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_27,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_28,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_29,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_30,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_31,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_32,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_33,
};

const int kDefaultImageWebsiteIDs[] = {
    IDS_LOGIN_DEFAULT_USER_WEBSITE,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_1,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_2,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_3,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_4,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_5,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_6,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_7,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_8,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_9,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_10,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_11,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_12,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_13,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_14,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_15,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_16,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_17,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_18,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_19,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_20,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_21,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_22,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_23,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_24,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_25,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_26,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_27,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_28,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_29,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_30,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_31,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_32,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_33,
};

const int kDefaultImagesCount = arraysize(kDefaultImageResourceIDs);

const int kFirstDefaultImageIndex = 19;

// The order and the values of these constants are important for histograms
// of different Chrome OS versions to be merged smoothly.
const int kHistogramImageFromCamera = 19;
const int kHistogramImageFromFile = 20;
const int kHistogramImageOld = 21;
const int kHistogramImageFromProfile = 22;
const int kHistogramVideoFromCamera = 23;
const int kHistogramVideoFromFile = 24;
const int kHistogramImagesCount = kDefaultImagesCount + 6;

namespace {

const char kDefaultUrlPrefix[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER_";
const char kZeroDefaultUrl[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER";

// IDs of default user image descriptions.
const int kDefaultImageDescriptions[] = {
    0,  // No description for deprecated user image 0.
    0,  // No description for deprecated user image 1.
    0,  // No description for deprecated user image 2.
    0,  // No description for deprecated user image 3.
    0,  // No description for deprecated user image 4.
    0,  // No description for deprecated user image 5.
    0,  // No description for deprecated user image 6.
    0,  // No description for deprecated user image 7.
    0,  // No description for deprecated user image 8.
    0,  // No description for deprecated user image 9.
    0,  // No description for deprecated user image 10.
    0,  // No description for deprecated user image 11.
    0,  // No description for deprecated user image 12.
    0,  // No description for deprecated user image 13.
    0,  // No description for deprecated user image 14.
    0,  // No description for deprecated user image 15.
    0,  // No description for deprecated user image 16.
    0,  // No description for deprecated user image 17.
    0,  // No description for deprecated user image 18.
    IDS_LOGIN_DEFAULT_USER_DESC_19,
    IDS_LOGIN_DEFAULT_USER_DESC_20,
    IDS_LOGIN_DEFAULT_USER_DESC_21,
    IDS_LOGIN_DEFAULT_USER_DESC_22,
    IDS_LOGIN_DEFAULT_USER_DESC_23,
    IDS_LOGIN_DEFAULT_USER_DESC_24,
    IDS_LOGIN_DEFAULT_USER_DESC_25,
    IDS_LOGIN_DEFAULT_USER_DESC_26,
    IDS_LOGIN_DEFAULT_USER_DESC_27,
    IDS_LOGIN_DEFAULT_USER_DESC_28,
    IDS_LOGIN_DEFAULT_USER_DESC_29,
    IDS_LOGIN_DEFAULT_USER_DESC_30,
    IDS_LOGIN_DEFAULT_USER_DESC_31,
    IDS_LOGIN_DEFAULT_USER_DESC_32,
    IDS_LOGIN_DEFAULT_USER_DESC_33,
};

// Returns a string consisting of the prefix specified and the index of the
// image if its valid.
std::string GetDefaultImageString(int index, const std::string& prefix) {
  if (index < 0 || index >= kDefaultImagesCount) {
    DCHECK(!base::SysInfo::IsRunningOnChromeOS());
    return std::string();
  }
  return base::StringPrintf("%s%d", prefix.c_str(), index);
}

// Returns true if the string specified consists of the prefix and one of
// the default images indices. Returns the index of the image in |image_id|
// variable.
bool IsDefaultImageString(const std::string& s,
                          const std::string& prefix,
                          int* image_id) {
  DCHECK(image_id);
  if (!base::StartsWith(s, prefix, base::CompareCase::SENSITIVE))
    return false;

  int image_index = -1;
  if (base::StringToInt(base::StringPiece(s.begin() + prefix.length(), s.end()),
                        &image_index)) {
    if (image_index < 0 || image_index >= kDefaultImagesCount)
      return false;
    *image_id = image_index;
    return true;
  }

  return false;
}

}  // namespace

std::string GetDefaultImageUrl(int index) {
  if (index == 0)
    return kZeroDefaultUrl;
  return GetDefaultImageString(index, kDefaultUrlPrefix);
}

bool IsDefaultImageUrl(const std::string& url, int* image_id) {
  if (url == kZeroDefaultUrl) {
    *image_id = 0;
    return true;
  }
  return IsDefaultImageString(url, kDefaultUrlPrefix, image_id);
}

const gfx::ImageSkia& GetDefaultImage(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kDefaultImageResourceIDs[index]);
}

base::string16 GetDefaultImageDescription(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  int string_id = kDefaultImageDescriptions[index];
  if (string_id)
    return l10n_util::GetStringUTF16(string_id);
  else
    return base::string16();
}

int GetDefaultImageHistogramValue(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  // Create a gap in histogram values for
  // [kHistogramImageFromCamera..kHistogramImageFromProfile] block to fit.
  if (index < kHistogramImageFromCamera)
    return index;
  return index + 6;
}

std::unique_ptr<base::ListValue> GetAsDictionary() {
  auto image_urls = base::MakeUnique<base::ListValue>();
  for (int i = default_user_image::kFirstDefaultImageIndex;
       i < default_user_image::kDefaultImagesCount; ++i) {
    auto image_data = base::MakeUnique<base::DictionaryValue>();
    image_data->SetString("url", default_user_image::GetDefaultImageUrl(i));
    image_data->SetString("author",
                          l10n_util::GetStringUTF16(
                              default_user_image::kDefaultImageAuthorIDs[i]));
    image_data->SetString("website",
                          l10n_util::GetStringUTF16(
                              default_user_image::kDefaultImageWebsiteIDs[i]));
    image_data->SetString("title",
                          default_user_image::GetDefaultImageDescription(i));
    image_urls->Append(std::move(image_data));
  }
  return image_urls;
}

}  // namespace default_user_image
}  // namespace chromeos
