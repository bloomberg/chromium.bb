// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/phased_step_runner.h"
#import "ios/clean/chrome/app/steps/application_step.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Step that provides a single feature.
@interface TestStep : NSObject<ApplicationStep>
@property(nonatomic) BOOL hasRun;
@property(nonatomic) NSString* providedFeature;
@property(nonatomic) NSMutableSet<TestStep*>* dependencies;
@end

@interface AsyncTestStep : TestStep
@end

@interface PhaseChangeTestStep : TestStep
@property(nonatomic) NSUInteger newPhase;
@end

@implementation TestStep

@synthesize providedFeature = _providedFeature;
@synthesize hasRun = _hasRun;
@synthesize dependencies = _dependencies;

- (instancetype)init {
  if ((self = [super init])) {
    _dependencies = [[NSMutableSet alloc] init];
  }
  return self;
}

- (NSArray<NSString*>*)providedFeatures {
  return @[ self.providedFeature ];
}

- (BOOL)synchronous {
  return YES;
}

- (NSArray<NSString*>*)requiredFeatures {
  NSMutableArray<NSString*>* deps = [[NSMutableArray alloc] init];
  for (TestStep* dependency in self.dependencies) {
    [deps addObject:dependency.providedFeature];
  }
  return deps;
}

- (void)checkDeps {
  std::string mt = [NSThread isMainThread] ? "(mt) " : "(ot) ";
  LOG(ERROR) << "Running " << mt << self.providedFeature.UTF8String;
  EXPECT_FALSE(self.hasRun);
  for (TestStep* dependency in self.dependencies) {
    EXPECT_TRUE(dependency.hasRun);
  }
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  EXPECT_NSEQ(feature, self.providedFeature);
  [self checkDeps];
  self.hasRun = YES;
}

@end

@implementation AsyncTestStep

- (BOOL)synchronous {
  return NO;
}

@end

@implementation PhaseChangeTestStep
@synthesize newPhase = _newPhase;

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  EXPECT_NSEQ(feature, self.providedFeature);
  [self checkDeps];
  context.phase = self.newPhase;
  self.hasRun = YES;
}

@end

using PhasedStepRunnerTest = PlatformTest;

TEST_F(PhasedStepRunnerTest, TestUnexecutedSteps) {
  TestStep* step1 = [[TestStep alloc] init];
  step1.providedFeature = @"feature_a";

  TestStep* task2 = [[TestStep alloc] init];
  task2.providedFeature = @"feature_b";

  TestStep* task3 = [[TestStep alloc] init];
  task3.providedFeature = @"feature_c";

  NSUInteger testPhase = 1;
  NSUInteger untestedPhase = 2;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:@[ step1, task2, task3 ]];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  [runner requireFeature:@"feature_b" forPhase:untestedPhase];
  EXPECT_FALSE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);

  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, testPhase);
  EXPECT_TRUE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);
}

// Simple dependency chain A-> B-> C
TEST_F(PhasedStepRunnerTest, TestSimpleDependencies) {
  TestStep* step1 = [[TestStep alloc] init];
  step1.providedFeature = @"feature_a";

  TestStep* task2 = [[TestStep alloc] init];
  task2.providedFeature = @"feature_b";

  TestStep* task3 = [[TestStep alloc] init];
  task3.providedFeature = @"feature_c";

  [step1.dependencies addObject:task2];
  [task2.dependencies addObject:task3];

  NSUInteger testPhase = 1;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:@[ step1, task2, task3 ]];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  EXPECT_FALSE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);

  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, testPhase);
  EXPECT_TRUE(step1.hasRun);
  EXPECT_TRUE(task2.hasRun);
  EXPECT_TRUE(task3.hasRun);
}

// Dependency graph: A->B; B->C; B->D, C->D.
TEST_F(PhasedStepRunnerTest, TestDependencyGraph) {
  TestStep* step1 = [[TestStep alloc] init];
  step1.providedFeature = @"feature_a";

  TestStep* task2 = [[TestStep alloc] init];
  task2.providedFeature = @"feature_b";

  TestStep* task3 = [[TestStep alloc] init];
  task3.providedFeature = @"feature_c";

  TestStep* task4 = [[TestStep alloc] init];
  task4.providedFeature = @"feature_d";

  [step1.dependencies addObject:task2];
  [step1.dependencies addObject:task3];
  [task2.dependencies addObject:task4];
  [task3.dependencies addObject:task4];

  NSUInteger testPhase = 1;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:@[ step1, task2, task3, task4 ]];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  EXPECT_FALSE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);
  EXPECT_FALSE(task4.hasRun);
  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, testPhase);
  EXPECT_TRUE(step1.hasRun);
  EXPECT_TRUE(task2.hasRun);
  EXPECT_TRUE(task3.hasRun);
  EXPECT_TRUE(task4.hasRun);
}

TEST_F(PhasedStepRunnerTest, TestOneAsyncAction) {
  AsyncTestStep* step1 = [[AsyncTestStep alloc] init];
  step1.providedFeature = @"feature_a";
  NSUInteger testPhase = 1;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addStep:step1];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  EXPECT_FALSE(step1.hasRun);
  runner.phase = testPhase;
  EXPECT_EQ(runner.phase, testPhase);
  EXPECT_TRUE(step1.hasRun);
}

TEST_F(PhasedStepRunnerTest, TestAsyncDependencies) {
  AsyncTestStep* step1 = [[AsyncTestStep alloc] init];
  step1.providedFeature = @"feature_a";

  TestStep* task2 = [[TestStep alloc] init];
  task2.providedFeature = @"feature_b";

  AsyncTestStep* task3 = [[AsyncTestStep alloc] init];
  task3.providedFeature = @"feature_c";

  [step1.dependencies addObject:task2];
  [task2.dependencies addObject:task3];

  NSUInteger testPhase = 1;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:@[ step1, task2, task3 ]];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  EXPECT_FALSE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);

  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, testPhase);
  EXPECT_TRUE(step1.hasRun);
  EXPECT_TRUE(task2.hasRun);
  EXPECT_TRUE(task3.hasRun);
}

TEST_F(PhasedStepRunnerTest, TestManyDependencies) {
  NSMutableArray<TestStep*>* tasks = [[NSMutableArray alloc] init];
  AsyncTestStep* root_task = [[AsyncTestStep alloc] init];
  root_task.providedFeature = @"feature_a";
  [tasks addObject:root_task];

  for (int i = 0; i < 5; i++) {
    AsyncTestStep* task = [[AsyncTestStep alloc] init];
    task.providedFeature = [NSString stringWithFormat:@"feature_%d_", i];
    [root_task.dependencies addObject:task];
    for (int j = 0; j < 5; j++) {
      TestStep* task2 = [[TestStep alloc] init];
      task2.providedFeature =
          [NSString stringWithFormat:@"feature_%d_%d", i, j];
      [task.dependencies addObject:task2];
      for (int k = 0; k < 5; k++) {
        AsyncTestStep* task3 = [[AsyncTestStep alloc] init];
        task3.providedFeature =
            [NSString stringWithFormat:@"feature_%d_%d_%d", i, j, k];
        [task2.dependencies addObject:task3];
        [tasks addObject:task3];
      }
      [tasks addObject:task2];
    }
    [tasks addObject:task];
  }

  for (int i = 0; i < 20; i++) {
    NSUInteger index1 = arc4random() % [tasks count];
    NSUInteger index2 = arc4random() % [tasks count];
    AsyncTestStep* atask = [[AsyncTestStep alloc] init];
    atask.providedFeature = [NSString stringWithFormat:@"feature_x_%d", i];
    [[tasks objectAtIndex:index1].dependencies addObject:atask];
    [[tasks objectAtIndex:index2].dependencies addObject:atask];
    [tasks addObject:atask];

    index1 = arc4random() % [tasks count];
    index2 = arc4random() % [tasks count];
    TestStep* stask = [[TestStep alloc] init];
    stask.providedFeature = [NSString stringWithFormat:@"feature_y_%d", i];
    [[tasks objectAtIndex:index1].dependencies addObject:stask];
    [[tasks objectAtIndex:index2].dependencies addObject:stask];
    [tasks addObject:stask];
  }

  NSUInteger testPhase = 1;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:tasks];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  for (TestStep* task in tasks) {
    EXPECT_FALSE(task.hasRun);
  }
  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, testPhase);
  for (TestStep* task in tasks) {
    EXPECT_TRUE(task.hasRun)
        << "Expected task " << task.providedFeature.UTF8String
        << " to have run";
  }
}

// Simple phase change
TEST_F(PhasedStepRunnerTest, TestPhaseChangeInStep) {
  TestStep* step1 = [[TestStep alloc] init];
  step1.providedFeature = @"feature_a";

  PhaseChangeTestStep* task2 = [[PhaseChangeTestStep alloc] init];
  task2.providedFeature = @"feature_b";

  TestStep* task3 = [[TestStep alloc] init];
  task3.providedFeature = @"feature_c";

  TestStep* task4 = [[TestStep alloc] init];
  task4.providedFeature = @"feature_d";

  [step1.dependencies addObject:task2];
  [task2.dependencies addObject:task3];

  NSUInteger testPhase = 1;
  NSUInteger otherPhase = 2;
  task2.newPhase = otherPhase;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:@[ step1, task2, task3, task4 ]];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_a" forPhase:testPhase];
  [runner requireFeature:@"feature_d" forPhase:otherPhase];

  EXPECT_FALSE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);
  EXPECT_FALSE(task4.hasRun);

  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, otherPhase);
  EXPECT_FALSE(step1.hasRun);
  EXPECT_TRUE(task2.hasRun);
  EXPECT_TRUE(task3.hasRun);
  EXPECT_TRUE(task4.hasRun);
}

// Sync phase change with async tasks
TEST_F(PhasedStepRunnerTest, TestPhaseChangeInStepWithAsync) {
  TestStep* step1 = [[AsyncTestStep alloc] init];
  step1.providedFeature = @"feature_a";

  TestStep* task2 = [[AsyncTestStep alloc] init];
  task2.providedFeature = @"feature_b";

  PhaseChangeTestStep* task3 = [[PhaseChangeTestStep alloc] init];
  task3.providedFeature = @"feature_c";

  TestStep* task4 = [[AsyncTestStep alloc] init];
  task4.providedFeature = @"feature_d";

  TestStep* task5 = [[AsyncTestStep alloc] init];
  task5.providedFeature = @"feature_e";

  TestStep* task6 = [[TestStep alloc] init];
  task6.providedFeature = @"feature_f";

  [task3.dependencies addObject:step1];
  [task3.dependencies addObject:task2];
  [task4.dependencies addObject:task3];
  [task5.dependencies addObject:task3];

  NSUInteger testPhase = 1;
  NSUInteger otherPhase = 2;
  task3.newPhase = otherPhase;

  PhasedStepRunner* runner = [[PhasedStepRunner alloc] init];
  [runner addSteps:@[ step1, task2, task3, task4, task5, task6 ]];
  EXPECT_NE(testPhase, runner.phase);
  [runner requireFeature:@"feature_d" forPhase:testPhase];
  [runner requireFeature:@"feature_e" forPhase:testPhase];
  [runner requireFeature:@"feature_f" forPhase:otherPhase];

  EXPECT_FALSE(step1.hasRun);
  EXPECT_FALSE(task2.hasRun);
  EXPECT_FALSE(task3.hasRun);
  EXPECT_FALSE(task4.hasRun);
  EXPECT_FALSE(task5.hasRun);
  EXPECT_FALSE(task6.hasRun);

  runner.phase = testPhase;

  EXPECT_EQ(runner.phase, otherPhase);
  EXPECT_TRUE(step1.hasRun);
  EXPECT_TRUE(task2.hasRun);
  EXPECT_TRUE(task3.hasRun);
  EXPECT_FALSE(task4.hasRun);
  EXPECT_FALSE(task5.hasRun);
  EXPECT_TRUE(task6.hasRun);
}
