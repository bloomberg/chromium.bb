// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_model_snapshot.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/tabs/tab.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface TabModelMock : NSObject<NSFastEnumeration> {
  base::scoped_nsobject<NSArray> tabs_;
}
@end

@implementation TabModelMock

- (id)initWithTabs:(NSArray*)tabs {
  if ((self = [super init])) {
    tabs_.reset([tabs retain]);
  }
  return self;
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id*)stackbuf
                                    count:(NSUInteger)len {
  return [tabs_ countByEnumeratingWithState:state objects:stackbuf count:len];
}

@end

namespace {

class TabModelSnapshotTest : public PlatformTest {
 protected:
  Tab* TabMock(NSString* tabId, NSString* url, double time) {
    id tabMock = [OCMockObject mockForClass:[Tab class]];
    [[[tabMock stub] andReturn:tabId] tabId];
    [[[tabMock stub] andReturn:url] urlDisplayString];
    [[[tabMock stub] andReturnValue:OCMOCK_VALUE(time)] lastVisitedTimestamp];
    return (Tab*)tabMock;
  }
};

TEST_F(TabModelSnapshotTest, TestSingleHash) {
  Tab* tab1 = TabMock(@"id1", @"url1", 12345.6789);
  Tab* tab2 = TabMock(@"id2", @"url1", 12345.6789);
  Tab* tab3 = TabMock(@"id1", @"url2", 12345.6789);
  Tab* tab4 = TabMock(@"id1", @"url1", 12345);

  // Same tab
  size_t hash1 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab1);
  size_t hash2 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab1);
  EXPECT_EQ(hash1, hash2);

  // Different ids
  size_t hash3 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab1);
  size_t hash4 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab2);
  EXPECT_NE(hash3, hash4);

  // Different urls
  size_t hash5 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab1);
  size_t hash6 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab3);
  EXPECT_NE(hash5, hash6);

  // Different timestamps
  size_t hash7 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab1);
  size_t hash8 = TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab4);
  EXPECT_NE(hash7, hash8);
}

TEST_F(TabModelSnapshotTest, TestSnapshotHashes) {
  Tab* tab1 = TabMock(@"id1", @"url1", 12345.6789);
  Tab* tab2 = TabMock(@"id2", @"url1", 12345.6789);

  base::scoped_nsobject<TabModelMock> tabModel(
      [[TabModelMock alloc] initWithTabs:@[ tab1, tab2 ]]);
  TabModelSnapshot tabModelSnapshot((TabModel*)tabModel.get());
  tabModel.reset();

  EXPECT_EQ(tabModelSnapshot.hashes().size(), 2UL);
  EXPECT_EQ(tabModelSnapshot.hashes()[0],
            TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab1));
  EXPECT_EQ(tabModelSnapshot.hashes()[1],
            TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(tab2));
}

}  // namespace
