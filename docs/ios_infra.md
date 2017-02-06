# Continuous build and test infrastructure for Chromium for iOS

See the [instructions] for how to check out and build Chromium for iOS.

The Chromium projects use buildbot for continuous integration. This doc starts
with an overview of the system, then gives detailed explanations about each
part.

[TOC]

## Overview

Commits are made using the [commit queue], which triggers a series of try jobs
to compile and test the proposed patch against Chromium tip of tree before
actually making the commit. If the try jobs succeed the patch is committed. A
newly committed change triggers the builders (or "bots") to compile and test
the change again.

## Bots

Bots are slaves attached to a buildbot master (or "waterfall"). A buildbot
master is a server which polls for commits to a repository and triggers workers
to compile and test new commits whenever they are detected. [chromium.mac] is
the main waterfall for Mac desktop and iOS. [tryserver.chromium.mac] serves
as the try server for Mac desktop and iOS.

The bots know how to check out a given revision of Chromium, compile, and test.

### Code location

#### Master configs

The masters are configured in [tools/build], a separate repository which
contains various infra-related scripts.

#### Pollers

[chromium.mac] uses a `GitilesPoller` which polls the Chromium repository for
new commits using the [gitiles] interface. When a new commit is detected, the
bots are triggered.

#### Recipes

The bots run [recipes], which are scripts that specify their sequence of steps
located in [tools/build]. An iOS-specific [recipe module] contains common
functionality that the various [iOS recipes] use.

#### Configs

Because the recipes live in another repository, changes to the recipes don't
go through the Chromium [commit queue] and aren't tested on the [try server].
In order to allow bot changes to be tested by the commit queue, the recipes
for iOS are generic instead of bot-specific, and rely on configuration files
which live in master-specific JSON config files located in [src/ios/build/bots].
These configs define the `gn_args` to use during compilation as well as the
tests to run.

#### Scripts

The [test runner] is the script which installs and runs the tests, interprets
the results, and collects any files emitted by the test ("test data"). It can
be found in [src/ios/build/bots/scripts], which means changes to the test runner
can be tested on the [try server].

### Compiling with goma

Goma is the distributed build system used by Chromium. It reduces compilation
time by avoiding recompilation of objects which have already been compiled
elsewhere.

### Testing with swarming

Tests run on [swarming], a distributed test system used by Chromium. After
compilation, configured tests will be zipped up along with their necessary
dependencies ("isolated") and sent to the [swarming server] for execution. The
server issues tasks to its attached workers for execution. The bots themselves
don't run any tests, they trigger tests to be run remotely on the swarming
server, then wait and display the results. This allows multiple tests to be
executed in parallel.

## Try bots

Try bots are bots which test proposed patches which are not yet committed.

Request [try job access] in order to trigger try jobs against your patch. The
relevant try bots for an iOS patch are `ios-device`, `ios-device-xcode-clang`,
`ios-simulator`, and `ios-simulator-xcode-clang`. These bots can be found on
the Mac-specific [try server]. A try job is said to succeed when the build
passes (i.e. when the bot successfully compiles and tests the patch).

`ios-device` and `ios-device-xcode-clang` both compile for the iOS device
architecture (ARM), and neither run any tests. A build is considered successful
so long as compilation is successful.

`ios-simulator` and `ios-simulator-xcode-clang` both compile for the iOS
simulator architecture (x86), and run tests in the iOS [simulator]. A build is
considered successful when both compilation and all configured test succeed.

`ios-device` and `ios-simulator` both compile using the version of [clang]
defined by the `CLANG_REVISION` in the Chromium tree.

`ios-device-xcode-clang` and `ios-simulator-xcode-clang` both compile using the
version of clang that ships with Xcode.

### Scheduling try jobs using buildbucket

Triggering a try job and collecting its results is accomplished using
[buildbucket]. The service allows for build requests to be put into buckets. A
request in this context is a set of properties indicating things such as where
to get the patch. The try bots are set up to poll a particular bucket for build
requests which they execute and post the results of.

### Compiling with the analyzer

In addition to goma, the try bots use another time-saving mechanism called the
[analyzer] to determine the subset of compilation targets affected by the patch
that need to be compiled in order to run the affected tests. If a patch is
determined not to affect a certain test target, compilation and execution of the
test target will be skipped.

[analyzer]: ../tools/mb
[buildbucket]: https://cr-buildbucket.appspot.com
[chromium.mac]: https://build.chromium.org/p/chromium.mac
[clang]: ../tools/clang
[commit queue]: https://dev.chromium.org/developers/testing/commit-queue
[gitiles]: https://gerrit.googlesource.com/gitiles
[instructions]: ./ios_build_instructions.md
[iOS recipes]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipes/ios
[recipe module]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/ios
[recipes]: https://chromium.googlesource.com/infra/infra/+/HEAD/doc/users/recipes.md
[simulator]: https://developer.apple.com/library/content/documentation/IDEs/Conceptual/iOS_Simulator_Guide/Introduction/Introduction.html
[src/ios/build/bots]: ../ios/build/bots
[src/ios/build/bots/scripts]: ../ios/build/bots/scripts
[swarming]: https://github.com/luci/luci-py/tree/master/appengine/swarming
[swarming server]: https://chromium-swarm.appspot.com
[test runner]: ../ios/build/bots/scripts/test_runner.py
[tools/build]: https://chromium.googlesource.com/chromium/tools/build
[try job access]: https://www.chromium.org/getting-involved/become-a-committer#TOC-Try-job-access
[try server]: https://build.chromium.org/p/tryserver.chromium.mac/waterfall
[tryserver.chromium.mac]: https://build.chromium.org/p/tryserver.chromium.mac/waterfall
