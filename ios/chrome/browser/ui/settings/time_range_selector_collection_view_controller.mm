// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/time_range_selector_collection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePastHour = kItemTypeEnumZero,
  ItemTypePastDay,
  ItemTypePastWeek,
  ItemTypeLastFourWeeks,
  ItemTypeBeginningOfTime,
};

const int kStringIDS[] = {
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_DAY,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME};

static_assert(arraysize(kStringIDS) == browsing_data::TIME_PERIOD_LAST + 1,
              "Strings have to match the enum values.");

}  // namespace

@interface TimeRangeSelectorCollectionViewController () {
  // Instance of the parent view controller needed in order to set the time
  // range for the browsing data deletion.
  base::WeakNSProtocol<id<TimeRangeSelectorCollectionViewControllerDelegate>>
      _weakDelegate;
  IntegerPrefMember timeRangePref_;
}

// Updates the checked state of the cells to match the preferences.
- (void)updateCheckedState;

// Updates the PrefService with the given value.
- (void)updatePrefValue:(int)prefValue;
@end

@implementation TimeRangeSelectorCollectionViewController

#pragma mark Initialization

- (instancetype)
initWithPrefs:(PrefService*)prefs
     delegate:(id<TimeRangeSelectorCollectionViewControllerDelegate>)delegate {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _weakDelegate.reset(delegate);
    self.title = l10n_util::GetNSString(
        IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
    timeRangePref_.Init(browsing_data::prefs::kDeleteTimePeriod, prefs);
    [self loadModel];
    self.shouldHideDoneButton = YES;
  }
  return self;
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastHour
                                 textMessageID:kStringIDS[0]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastDay
                                 textMessageID:kStringIDS[1]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastWeek
                                 textMessageID:kStringIDS[2]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypeLastFourWeeks
                                 textMessageID:kStringIDS[3]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypeBeginningOfTime
                                 textMessageID:kStringIDS[4]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [self updateCheckedState];
}

- (void)updateCheckedState {
  int timeRangePrefValue = timeRangePref_.GetValue();
  CollectionViewModel* model = self.collectionViewModel;

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (CollectionViewTextItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierOptions]) {
    NSInteger itemPrefValue = item.type - kItemTypeEnumZero;

    MDCCollectionViewCellAccessoryType desiredType =
        itemPrefValue == timeRangePrefValue
            ? MDCCollectionViewCellAccessoryCheckmark
            : MDCCollectionViewCellAccessoryNone;
    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems
         inSectionWithIdentifier:SectionIdentifierOptions];
}

- (void)updatePrefValue:(int)prefValue {
  timeRangePref_.SetValue(prefValue);
  [self updateCheckedState];
}

- (CollectionViewTextItem*)timeRangeItemWithOption:(ItemType)itemOption
                                     textMessageID:(int)textMessageID {
  CollectionViewTextItem* item =
      [[[CollectionViewTextItem alloc] initWithType:itemOption] autorelease];
  [item setText:l10n_util::GetNSString(textMessageID)];
  [item setAccessibilityTraits:UIAccessibilityTraitButton];
  return item;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  int timePeriod = itemType - kItemTypeEnumZero;
  DCHECK_LE(timePeriod, browsing_data::TIME_PERIOD_LAST);
  [self updatePrefValue:timePeriod];
  [_weakDelegate
      timeRangeSelectorViewController:self
                  didSelectTimePeriod:static_cast<browsing_data::TimePeriod>(
                                          timePeriod)];
}

#pragma mark - Class methods

+ (NSString*)timePeriodLabelForPrefs:(PrefService*)prefs {
  if (!prefs)
    return nil;
  int prefValue = prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod);
  if (prefValue < 0 || static_cast<size_t>(prefValue) >= arraysize(kStringIDS))
    return nil;
  return l10n_util::GetNSString(kStringIDS[prefValue]);
}

@end
