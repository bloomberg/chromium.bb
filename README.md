# chromium.bb

This repository contains the version of Chromium used by Bloomberg, along with
all the modifications made by Bloomberg in order to use it in our environment.

This repository serves a couple of purposes for us:

* **Provide a monolithic repo of the Chromium project**
  A typical Chromium checkout consists of many "submodule" repositories.  In
contrast, this repo contains all of the submodules from the original repo
but in the form of "subtree".  Having a single repository for all of Chromium
code is much easier for us to use internally.

* **Provide a space for us to make/publish changes**
  We have made a bunch of changes to different parts of Chromium in order to
make it behave the way we need it to.  Some of these changes are bugfixes that
can be submitted upstream, and some of them are just changes that we
specifically wanted for our product, but may not be useful or desirable
upstream.  Each change is made on a separate branch.  This allows us, at
release time, to pick and choose which bugfixes/features we want to include in
the release.

  Note that while most of our bugfixes are not submitted upstream, it is our
intention to submit as many bugfixes upstream as we can.

  The list of branches along with descriptions and test cases can be found
[here](https://github.com/bloomberg/chromium.bb/blob/gh-pages/README.md).


## Branch Structure

The `master` branch contains the code that is used to make the official
Chromium builds used by Bloomberg.  It is not used for development.  Actual
development happens in one of the `bugfix/*`, `feature/*` or `blpwtk2/*`
branches.

Each `bugfix/*`, `feature/*` or `blpwtk2/*` branch is based on the
`patched/<cr-version>` branch, which contains the source tree of upstream
Chromium project.

The `release/candidate` branch contains changes that are scheduled to be
included in the next release.


## Build Instructions

**Bloomberg Employees:** [Build steps](https://cms.prod.bloomberg.com/team/display/rwf/Maintenance+of+blpwtk2)

If you are **not** a Bloomberg employee, the following instructions should still
work:

* Setup your build environment:
    * [Python 2.7](https://www.python.org/download/releases/2.7.6/)
    * Visual Studio 2017
    * [depot_tools](https://dev.chromium.org/developers/how-tos/depottools)
    * Windows SDK 10.0.17134

* Run the following commands from inside the `/src` directory:

            build/runhooks.py
            build/blpwtk2.py

* Run ninja to build:

            ninja -C src/out/shared_debug blpwtk2_all     # for Debug component builds
            ninja -C src/out/shared_release  blpwtk2_all  # for Release component builds
            ninja -C src/out/static_debug  blpwtk2_all    # for Debug static builds
            ninja -C src/out/static_release  blpwtk2_all  # for Release static builds

---
###### Microsoft, Windows, Visual Studio and ClearType are registered trademarks of Microsoft Corp.
###### Firefox is a registered trademark of Mozilla Corp.
###### Chrome is a registered trademark of Google Inc.
