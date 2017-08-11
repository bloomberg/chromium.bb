// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/phased_step_runner.h"

#import "base/logging.h"
#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// An ApplicationStep used to anchor the dependency graph for a phase change.
// PhasedStepRunner will create a PhaseChangeStep for each phase that has
// required features. All other steps for that phase change will be dependencies
// of the PhaseChangeStep. Running the PhaseChangeStep will set the
// PhasedStepRunner's |running| property to NO.
@interface PhaseChangeStep : NSObject<ApplicationStep>
// |runner| is the PhasedStepRunner whose |running| property should be unset
// when this step runs.
// |phase| is the phase that the PhasedStepRunner is expected to be in when
// this step is run.
- (instancetype)initWithRunner:(PhasedStepRunner*)runner
                         phase:(NSUInteger)phase NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
// Phase set in -initWithRunner:phase:
@property(nonatomic, readonly) NSUInteger phase;
// Runner set in -initWithRunner:phase:
@property(nonatomic, weak) PhasedStepRunner* runner;
// Features required for this phase.
@property(nonatomic, readonly) NSMutableArray<NSString*>* requiredFeatures;
@end

// NSOperation subclass used for jobs.
@interface StepOperation : NSOperation
- (instancetype)initWithStep:(id<ApplicationStep>)step
                     feature:(NSString*)feature
                     context:(id<StepContext>)context NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@property(nonatomic, copy) NSString* feature;
@property(nonatomic, readonly, weak) id<ApplicationStep> step;
@property(nonatomic, readonly, weak) id<StepContext> context;
@end

@interface PhasedStepRunner ()
+ (NSString*)featureForChangeToPhase:(NSUInteger)phase;
@property(nonatomic)
    NSMutableDictionary<NSString*, id<ApplicationStep>>* featureProviders;
@property(nonatomic) NSOperationQueue* queue;
@property(nonatomic) NSMutableSet<StepOperation*>* jobs;
@property(nonatomic, readonly) NSSet<StepOperation*>* readyJobs;
@property(nonatomic) BOOL running;
@end

@implementation PhaseChangeStep
@synthesize phase = _phase;
@synthesize runner = _runner;
@synthesize providedFeatures = _providedFeatures;
@synthesize requiredFeatures = _requiredFeatures;
@synthesize synchronous = _synchronous;

- (instancetype)initWithRunner:(PhasedStepRunner*)runner
                         phase:(NSUInteger)phase {
  if ((self = [super init])) {
    _runner = runner;
    _phase = phase;
    _providedFeatures = @[ [PhasedStepRunner featureForChangeToPhase:_phase] ];
    _requiredFeatures = [[NSMutableArray alloc] init];
    // Always synchronous.
    _synchronous = YES;
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  self.runner.running = NO;
}

@end

@implementation StepOperation
@synthesize step = _step;
@synthesize feature = _feature;
@synthesize context = _context;

- (instancetype)initWithStep:(id<ApplicationStep>)step
                     feature:(NSString*)feature
                     context:(id<StepContext>)context {
  if ((self = [super init])) {
    _step = step;
    _feature = feature;
    _context = context;
    self.name = feature;
  }
  return self;
}

- (void)main {
  [self.step runFeature:self.feature withContext:self.context];
}

@end

@implementation PhasedStepRunner

@synthesize phase = _phase;
@synthesize featureProviders = _featureProviders;
@synthesize jobs = _jobs;
@synthesize queue = _queue;
@synthesize running = _running;
@synthesize URLOpener = _URLOpener;

- (instancetype)init {
  if ((self = [super init])) {
    _featureProviders = [[NSMutableDictionary alloc] init];
    _jobs = [[NSMutableSet alloc] init];
  }
  return self;
}

+ (NSString*)featureForChangeToPhase:(NSUInteger)phase {
  return [NSString stringWithFormat:@"__PhaseChangeCompleteForPhase_%ld",
                                    static_cast<unsigned long>(phase)];
}

- (void)addStep:(id<ApplicationStep>)step {
  for (NSString* feature in step.providedFeatures) {
    DCHECK(!self.featureProviders[feature])
        << "Step provides duplicated feature.";
    self.featureProviders[feature] = step;
  }
}

- (void)addSteps:(NSArray<id<ApplicationStep>>*)steps {
  for (id<ApplicationStep> step in steps) {
    [self addStep:step];
  }
}

- (void)requireFeature:(NSString*)feature forPhase:(NSUInteger)phase {
  NSString* phaseFeature = [[self class] featureForChangeToPhase:phase];
  PhaseChangeStep* changeStep =
      base::mac::ObjCCast<PhaseChangeStep>(self.featureProviders[phaseFeature]);
  if (!changeStep) {
    changeStep = [[PhaseChangeStep alloc] initWithRunner:self phase:phase];
    [self addStep:changeStep];
  }
  [changeStep.requiredFeatures addObject:feature];
}

- (void)setPhase:(NSUInteger)phase {
  if (phase == _phase)
    return;
  _phase = phase;
  if (self.running) {
    [self stop];
  } else {
    [self run];
  }
}

- (NSSet<NSOperation*>*)readyJobs {
  return [self.jobs objectsPassingTest:^BOOL(NSOperation* job, BOOL* stop) {
    return job.ready && !job.executing;
  }];
}

- (NSArray<StepOperation*>*)jobsForCurrentPhase {
  // Build the jobs.
  LOG(ERROR) << "Building jobs for phase " << self.phase;
  NSMutableDictionary<NSString*, StepOperation*>* featureJobs =
      [[NSMutableDictionary alloc] init];
  NSMutableSet<NSString*>* requiredFeatures = [NSMutableSet<NSString*>
      setWithObject:[[self class] featureForChangeToPhase:self.phase]];

  while (requiredFeatures.count) {
    NSString* feature = [requiredFeatures anyObject];
    id<ApplicationStep> step = self.featureProviders[feature];
    DCHECK(step) << "No provider for feature " << feature.UTF8String;
    if (!featureJobs[feature]) {
      featureJobs[feature] = [[StepOperation alloc] initWithStep:step
                                                         feature:feature
                                                         context:self];
    }
    [requiredFeatures removeObject:feature];
    for (NSString* requirement in step.requiredFeatures) {
      [requiredFeatures addObject:requirement];
    }
  }

  // Set up dependencies.
  for (NSString* feature in featureJobs.allKeys) {
    for (NSString* requirement in self.featureProviders[feature]
             .requiredFeatures) {
      [featureJobs[feature] addDependency:featureJobs[requirement]];
    }
  }

  LOG(ERROR) << "Created " << featureJobs.allValues.count << " jobs";
  return featureJobs.allValues;
}

- (void)stop {
  LOG(ERROR) << "Stopping; " << self.jobs.count << " sync jobs, "
             << self.queue.operationCount << " async";
  [self.jobs removeAllObjects];
  [self.queue cancelAllOperations];
  [self.queue waitUntilAllOperationsAreFinished];
  self.running = NO;
}

// Run all of the steps for the current phase.
- (void)run {
  [self.jobs removeAllObjects];
  self.queue = [[NSOperationQueue alloc] init];
  self.running = YES;
  for (StepOperation* job in [self jobsForCurrentPhase]) {
    if (job.step.synchronous)
      [self.jobs addObject:job];
    else
      [self.queue addOperation:job];
  }

  // Watch all of the jobs running synchronously on this thread.
  for (NSOperation* job in self.jobs) {
    [job addObserver:self
          forKeyPath:@"isFinished"
             options:NSKeyValueObservingOptionNew
             context:nullptr];
  }

  // Run the synchronous jobs.
  LOG(ERROR) << "Running " << self.jobs.count << " sync jobs for phase "
             << self.phase;
  NSUInteger initialPhase = self.phase;
  while (self.jobs.count) {
    [[self.readyJobs anyObject] start];
  }
  DCHECK_EQ(self.queue.operationCount, 0UL);
  DCHECK(!self.running);
  self.queue = nil;

  // If the phase was changed while running the jobs, then start over again.
  if (initialPhase != self.phase) {
    [self run];
  }
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  StepOperation* job = base::mac::ObjCCast<StepOperation>(object);
  if (!job)
    return;
  if (job.finished) {
    // When a job finishes, remove it.
    [self.jobs removeObject:job];
    // Also stop observing it.
    [job removeObserver:self forKeyPath:@"isFinished"];
  }
}

@end
