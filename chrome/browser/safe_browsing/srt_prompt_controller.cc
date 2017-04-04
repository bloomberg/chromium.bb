// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_prompt_controller.h"

#include <initializer_list>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace safe_browsing {

namespace {

// Some dummy strings to be displayed in the Cleaner dialog while iterating on
// the dialog's UX design and work on the Chrome<->Cleaner IPC is ongoing.
constexpr char kMainTextIntroduction[] =
    "Have you seen unusual startup pages or strange search results? Chrome has "
    "detected the following kinds of programs on your computer that may be "
    "causing the problem:";
constexpr char kMainTextActionExplanation[] =
    "Chrome can remove the detected programs, which should stop the strange "
    "behavior.";
constexpr char kUnwantedSoftwareCategory1[] = "1 Browser hijacker";
constexpr char kUnwantedSoftwareCategory2[] = "2 Ad Injectors";

constexpr char kDetailsSectionSettingsResetExplanation[] =
    "Chrome will reset the following settings:";
constexpr char kDetailsSectionSetting1[] = "Default search engine";
constexpr char kDetailsSectionSetting2[] = "Startup pages";
constexpr char kDetailsSectionSetting3[] = "Homepage";
constexpr char kDetailsSectionSetting4[] = "Shortcuts";
constexpr char kDetailsSectionSetting5[] =
    "All extensions (these can be enabled again later)";
constexpr char kDetailsSectionActionExplanation[] =
    "The following files will be removed:";
constexpr char kDetailsSectionPoweredBy[] = "Powered by: <ESET logo>";

constexpr char kDummyDirectory[] =
    "C:\\Documents and Settings\\JohnDoe\\Local Settings\\Application Data"
    "\\IAmNotWanted\\application\\";
constexpr char kDummyFilename1[] = "somefile.dll";
constexpr char kDummyFilename2[] = "another_file.dll";
constexpr char kDummyFilename3[] = "more_stuff.dll";
constexpr char kDummyFilename4[] = "run_me.exe";

constexpr char kShowDetails[] = "Learn more";
constexpr char kHideDetails[] = "Close";
constexpr char kWindowTitle[] = "Chrome detected unusual behavior";
constexpr char kAcceptButtonLabel[] = "Start cleanup";

}  // namespace

SRTPromptController::LabelInfo::LabelInfo(LabelType type,
                                          const base::string16& text)
    : type(type), text(text) {}

SRTPromptController::LabelInfo::~LabelInfo() = default;

SRTPromptController::SRTPromptController() {}

SRTPromptController::~SRTPromptController() = default;

base::string16 SRTPromptController::GetWindowTitle() const {
  return base::UTF8ToUTF16(kWindowTitle);
}

std::vector<SRTPromptController::LabelInfo> SRTPromptController::GetMainText()
    const {
  return {
      LabelInfo(LabelInfo::PARAGRAPH, base::UTF8ToUTF16(kMainTextIntroduction)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kUnwantedSoftwareCategory1)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kUnwantedSoftwareCategory2)),
      LabelInfo(LabelInfo::PARAGRAPH,
                base::UTF8ToUTF16(kMainTextActionExplanation)),
  };
}

std::vector<SRTPromptController::LabelInfo>
SRTPromptController::GetDetailsText() const {
  return {
      LabelInfo(LabelInfo::PARAGRAPH,
                base::UTF8ToUTF16(kDetailsSectionSettingsResetExplanation)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kDetailsSectionSetting1)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kDetailsSectionSetting2)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kDetailsSectionSetting3)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kDetailsSectionSetting4)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(kDetailsSectionSetting5)),
      LabelInfo(LabelInfo::PARAGRAPH,
                base::UTF8ToUTF16(kDetailsSectionActionExplanation)),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(base::JoinString(
                    {kDummyDirectory, kDummyFilename1}, nullptr))),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(base::JoinString(
                    {kDummyDirectory, kDummyFilename2}, nullptr))),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(base::JoinString(
                    {kDummyDirectory, kDummyFilename3}, nullptr))),
      LabelInfo(LabelInfo::BULLET_ITEM,
                base::UTF8ToUTF16(base::JoinString(
                    {kDummyDirectory, kDummyFilename4}, nullptr))),
      LabelInfo(LabelInfo::PARAGRAPH,
                base::UTF8ToUTF16(kDetailsSectionPoweredBy)),
  };
}

base::string16 SRTPromptController::GetShowDetailsLabel() const {
  return base::UTF8ToUTF16(kShowDetails);
}

base::string16 SRTPromptController::GetHideDetailsLabel() const {
  return base::UTF8ToUTF16(kHideDetails);
}

base::string16 SRTPromptController::GetAcceptButtonLabel() const {
  return base::UTF8ToUTF16(kAcceptButtonLabel);
}

void SRTPromptController::DialogShown() {}

void SRTPromptController::Accept() {
  OnInteractionDone();
}

void SRTPromptController::Cancel() {
  OnInteractionDone();
}

void SRTPromptController::OnInteractionDone() {
  delete this;
}

}  // namespace safe_browsing
