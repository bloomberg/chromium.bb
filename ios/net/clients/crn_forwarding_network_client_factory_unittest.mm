// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/clients/crn_forwarding_network_client_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface TestFactoryA : CRNForwardingNetworkClientFactory
@end
@implementation TestFactoryA
@end

// B must appear before A
@interface TestFactoryB : CRNForwardingNetworkClientFactory
@end
@implementation TestFactoryB
+ (instancetype)mustApplyBefore { return [TestFactoryA class]; }
@end

// C must appear after A
@interface TestFactoryC : CRNForwardingNetworkClientFactory
@end
@implementation TestFactoryC
+ (instancetype)mustApplyAfter { return [TestFactoryA class]; }
@end

// D must appear after B and before C
@interface TestFactoryD : CRNForwardingNetworkClientFactory
@end
@implementation TestFactoryD
+ (instancetype)mustApplyAfter { return [TestFactoryB class]; }
+ (instancetype)mustApplyBefore { return [TestFactoryC class]; }
@end

// E and F form a loop
@interface TestFactoryE : CRNForwardingNetworkClientFactory
@end
@interface TestFactoryF : CRNForwardingNetworkClientFactory
@end
@implementation TestFactoryE
+ (instancetype)mustApplyAfter { return [TestFactoryF class]; }
@end
@implementation TestFactoryF
+ (instancetype)mustApplyAfter { return [TestFactoryE class]; }
@end

// G must appear before B and after C, so while not a loop, it can't be
// ordered consistently.
@interface TestFactoryG : CRNForwardingNetworkClientFactory
@end
@implementation TestFactoryG
+ (instancetype)mustApplyAfter { return [TestFactoryC class]; }
+ (instancetype)mustApplyBefore { return [TestFactoryB class]; }
@end

namespace {

class ForwardingNetworkClientFactoryTest : public testing::Test {
 public:
  ForwardingNetworkClientFactoryTest() {}

  void SetUp() override {
    factory_a_.reset([[TestFactoryA alloc] init]);
    factory_b_.reset([[TestFactoryB alloc] init]);
    factory_c_.reset([[TestFactoryC alloc] init]);
    factory_d_.reset([[TestFactoryD alloc] init]);
  }

 protected:
  base::scoped_nsobject<TestFactoryA> factory_a_;
  base::scoped_nsobject<TestFactoryB> factory_b_;
  base::scoped_nsobject<TestFactoryC> factory_c_;
  base::scoped_nsobject<TestFactoryD> factory_d_;
};

}  // namespace

TEST_F(ForwardingNetworkClientFactoryTest, SortFactories) {
  base::scoped_nsobject<NSMutableArray> array([[NSMutableArray alloc] init]);

  // Add a, c, b.
  EXPECT_TRUE([[TestFactoryA class] orderedOK]);
  EXPECT_TRUE([[TestFactoryB class] orderedOK]);
  EXPECT_TRUE([[TestFactoryC class] orderedOK]);
  [array addObject:factory_a_.get()];
  [array addObject:factory_b_.get()];
  [array addObject:factory_c_.get()];
  [array sortUsingSelector:@selector(orderRelativeTo:)];
  // Expect b before a.
  EXPECT_LT([array indexOfObject:factory_b_.get()],
            [array indexOfObject:factory_a_.get()]);
  // Expect c after a.
  EXPECT_GT([array indexOfObject:factory_c_.get()],
            [array indexOfObject:factory_a_.get()]);

  // Add d.
  EXPECT_TRUE([[TestFactoryD class] orderedOK]);
  [array addObject:factory_d_.get()];
  [array sortUsingSelector:@selector(orderRelativeTo:)];
  // Expect previous relations unchanged.
  EXPECT_LT([array indexOfObject:factory_b_.get()],
            [array indexOfObject:factory_a_.get()]);
  EXPECT_GT([array indexOfObject:factory_c_.get()],
            [array indexOfObject:factory_a_.get()]);
  // Expect b before d.
  EXPECT_LT([array indexOfObject:factory_b_.get()],
            [array indexOfObject:factory_d_.get()]);
  // Expect c after d.
  EXPECT_GT([array indexOfObject:factory_c_.get()],
            [array indexOfObject:factory_d_.get()]);

  // E and F are not OK.
  EXPECT_FALSE([[TestFactoryE class] orderedOK]);
  EXPECT_FALSE([[TestFactoryF class] orderedOK]);

  // G is not OK.
  EXPECT_FALSE([[TestFactoryG class] orderedOK]);
}

TEST_F(ForwardingNetworkClientFactoryTest, TestSubclassImplementations) {
  // Look at all the subclasses of of CRNForwardingNetworkClientFactory.
  // Make sure that each one implements at least one clientHandling.. method.
  Class factory_superclass = [CRNForwardingNetworkClientFactory class];
  std::string superclass_name = class_getName(factory_superclass);

  base::scoped_nsobject<NSMutableArray> subclasses(
      [[NSMutableArray alloc] init]);

  // Look at every known class and find those that are subclasses of
  // |factory_superclass|.
  int class_count = objc_getClassList(nullptr, 0);
  Class* classes = nullptr;
  classes = static_cast<Class*>(malloc(sizeof(Class) * class_count));
  class_count = objc_getClassList(classes, class_count);

  for (NSInteger i = 0; i < class_count; i++) {
    std::string class_name = class_getName(classes[i]);
    // Skip the test classes defined above.
    if (StartsWithASCII(class_name, "TestFactory", false))
      continue;

    Class subclass_super = classes[i];
    int subclassing_count = 0;
    // Walk up the class hiererchy from |classes[i]| to |factory_superclass|.
    do {
      subclass_super = class_getSuperclass(subclass_super);
      subclassing_count++;
    } while (subclass_super && subclass_super != factory_superclass);

    if (subclass_super == nil)
      continue;

    // If |subclassing_count| > 1 we have found a class that is a subclass of
    // a subclass of |factory_superclass|, which we want to (for now) flag.
    EXPECT_EQ(1, subclassing_count)
        << class_name << " is an indirect subclass of " << superclass_name;

    [subclasses addObject:classes[i]];
  }

  // Get all of the methods defined in ForwardingNetworkClientFactoryTest and
  // compile a list of those named "clientHandling...".
  base::scoped_nsobject<NSMutableArray> client_handling_methods(
      [[NSMutableArray alloc] init]);
  unsigned int method_count;
  Method* superclass_methods =
      class_copyMethodList(factory_superclass, &method_count);
  for (unsigned int i = 0; i < method_count; i++) {
    NSString* method_name =
        NSStringFromSelector(method_getName(superclass_methods[i]));
    if ([method_name hasPrefix:@"clientHandling"]) {
      [client_handling_methods addObject:method_name];
    }
  }
  free(superclass_methods);

  // The superclass should implement at least one method.
  EXPECT_LT(0u, [client_handling_methods count]);

  for (Class subclass in subclasses.get()) {
    Method* methods = class_copyMethodList(subclass, &method_count);
    // The subclass has to have > 0 methods
    EXPECT_LT(0u, method_count);

    // Collect an array of the methods the class implements, then check each
    // superclass clientHandling method to see if it's in the list.
    base::scoped_nsobject<NSMutableArray> method_names(
        [[NSMutableArray alloc] init]);
    for (unsigned int i = 0; i < method_count; i++) {
      [method_names addObject:NSStringFromSelector(method_getName(methods[i]))];
    }
    free(methods);

    base::scoped_nsobject<NSMutableArray> subclass_implementations(
        [[NSMutableArray alloc] init]);
    for (NSString* superclass_method_name in client_handling_methods.get()) {
      if ([method_names indexOfObject:superclass_method_name] != NSNotFound) {
        [subclass_implementations addObject:superclass_method_name];
      }
    }

    std::string class_name = class_getName(subclass);
    std::string method_list = base::SysNSStringToUTF8(
        [subclass_implementations componentsJoinedByString:@", "]);
    ASSERT_NE(0u, [subclass_implementations count])
        << class_name
        << " does not implement any required clientHandling methods.";
    ASSERT_EQ(1u, [subclass_implementations count])
        << class_name << " implements too many superclass methods ("
        << method_list << ").";
  }

  free(classes);
}
