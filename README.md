# Chromite Development: Starter Guide

[TOC]

## Objective
This doc tries to give an overview and head start to anyone just starting out on Chromite development.

## Background
Before you get started on Chromite, we recommend that you go through ChromeOS developer guides at [external (first)](https://chromium.googlesource.com/chromiumos/docs/+/master/developer_guide.md) and then [goto/chromeos-building](http://goto/chromeos-building) for internal. The [Gerrit starter guide](https://sites.google.com/a/google.com/android/development/repo-gerrit-git-workflow) may also be helpful. You should flash a built image on a test device (Ask around for one!).

Chromite was intended to be the unified codebase for anything related to building ChromeOS/ChromiumOS. Currently, it is the codebase responsible for several things including:  building the OS from the requisite packages for the necessary board (`parallel_emerge`), driving the infrastructure build workflow (CBuildBot), hosting a Google App Engine App, and providing utility functions for various scripts scattered around ChromeOS repositories. It is written for the most part in Python with some Bash sprinkled in.

## Directory Overview
You can use [Code Search](https://cs.corp.google.com/) to lookup things in Chromite or ChromeOS in general. You can add a ChromeOS filter to only show files from CrOS repositories by going to [CS Settings](https://cs.corp.google.com/settings/) and adding a new Saved query: “`package:^chromeos`” named “chromeos”.

### `chromite/cbuildbot`
CBuildBot is the collection of entire code that runs on both the parent and the child build machines. It kicks off the individual stages in a particular build. It is a configurable bot that builds ChromeOS. More details on CBuildBot can be found in [this tech talk](https://drive.google.com/a/google.com/file/d/0BwPS_JpKyELWR2k0Z3JSWUhPSEE/view) ([slides](https://docs.google.com/presentation/d/1nUZFCAADgPp48SmrAFZVV_ngR27BdhKjL32nyu_hbOo/edit#slide=id.i0)).

### `chromite/cbuildbot/builders`
This folder contains configurations of the different builders in use. Each has its own set of stages to run usually called under RunStages function. Most builders used regularly are derived from SimpleBuilder class.

### `chromite/cbuildbot/stages`
Each file here has implementations of stages in the build process grouped by similarity. Each stage usually has PerformStage as its primary function.

### `chromite/lib`
Code here is expected to be imported whenever necessary throughout Chromite.

### `chromite/scripts`
Unlike lib, code in scripts will not and should not be imported anywhere. Instead they are executed as required in the build process. Each executable is linked to either `wrapper.py` or `virtualenv_wrapper.py`. Some of these links are in `chromite/bin`. The wrapper figures out the directory of the executable script and the `$PYTHONPATH`. Finally, it invokes the correct Python installation by moving up the directory structure to find which git repo is making the call.

### `chromite/third_party`
This folder contains all the third_party python libraries required by Chromite. You need a very strong reason to add any library to the current list. Please confirm with the owners beforehand.

### `chromite/*`
There are smaller folders with miscellaneous functions like config, licencing, cidb, etc.

## Testing your Chromite changes
Before any testing, you should check your code for lint errors with:

```shell
$ cros lint <filename>
```

### Unit Tests
Every Python file in Chromite is accompanied by a corresponding `filename_unittest.py` file. More on unit tests here. Once written, the unit tests can be run using `cbuildbot/run_tests` command in the Chromite directory. To test a specific file (say `lib/triage_lib.py`), use

```shell
~/trunk/chromite $ cbuildbot/run_tests lib/triage_lib_unittest
```

Run_tests without any argument runs all unit tests in Chromite. These unit tests are run in tryjobs, preCQ and CQ as well.

If you have to create a new Python file in Chromite, you should also create a `{filename}_unittest.py` in the same directory with all the unit tests. Also make a link called `{filename}_unittest` to `/mnt/host/source/chromite/scripts/wrapper.py`. See the other unittest files around if unclear.

### Tryjob
You can also fire a build on a server (or even locally) to have an entire build happen similar to how it would in Commit Queue.

```shell
$ cros tryjob -g <gerrit-change-id> <trybot-config>
$ cros tryjob -h -> for help on more options
```

Add `--hwtest` to add hardware testing to your tryjob. You can use the link provided by the command to check the status of your tryjob. Alternatively, you can go to the [CI UI tryjobs page](https://cros-goldeneye.corp.google.com/chromeos/legoland/builderSummary?buildConfig&builderGroups=tryjob&email) and filter results by your email.

### Pre-CQ
Once you mark your CL as Trybot-ready on [Chromium Gerrit](https://chromium-review.googlesource.com), the PreCQ will pick up your change and fire few preset config runs as a precursor to CQ. Currently, it doesn’t include any hardware or VM testing (for now!).

### Commit Queue
This is the final step in getting your change pushed. CQ is the most comprehensive of all tests. There are a multitude of CL's being validated in the same CQ. Once a CL is verified by CQ, it is merged into the codebase.

## How does ChromeOS build work?
Refer to these [talk slides](https://docs.google.com/presentation/d/1q8POSy8-LgqVvZu37KeXdd2-6F_4CpnfPzqu1fDlnW4) on ChromeOS Build Overview.
