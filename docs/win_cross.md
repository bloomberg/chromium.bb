# Cross-compiling Chrome/win

It's possible to build parts of the codebase on a Linux (and soon, Mac) host
while targeting Windows.  This document describes how to set that up, and
current restrictions.

What does *not* work:

* goma. Sorry. ([internal bug](b/64390790))
* targets depending on crashpad ([bug](https://crbug.com/762167))
* linking on Mac ([bug](https://crbug.com/774209)), should change soon

This disqualifies most interesting targets for now, but a few smaller ones
(`base_unittests`, ...) do work.  Over time, more things should work.

## .gclient setup

1. Tell gclient that you need Windows build dependencies by adding
   `target_os = ['win']` to the end of your `.gclient`.  (If you already
   have a `target_os` line in there, just add `'win'` to the list.)
1. `gclient sync`, follow instructions on screen.

If you're at Google, this will automatically download the Windows SDK for you.
If you are not at Google, you'll have to figure out how to get the SDK, and
you'll need to put a JSON file describing the SDK layout in a certain location.

# GN setup

Add `target_os = "win"` to your args.gn.  Then just build, e.g.

    ninja -C out/gnwin base_unittests.exe

# Running tests on swarming

You can run the Windows binaries you built on swarming, like so:

    tools/mb/mb.py isolate //out/gnwin base_unittests
    tools/swarming_client/isolate.py archive \
        -I https://isolateserver.appspot.com \
        -i out/gnwin/base_unittests.isolate \
        -s out/gnwin/base_unittests.isolated
    tools/swarming_client/swarming.py trigger \
        -S https://chromium-swarm.appspot.com \
        -I https://isolateserver.appspot.com \
        -d os Windows -d pool Chrome -s <hash printed by previous command>

Most tests that build should pass.  However, the cross build uses
the lld linker, and a couple of tests fail when using lld. You can look at
https://build.chromium.org/p/chromium.clang/builders/CrWinClangLLD%20tester
to get an idea of which tests fail with lld.

TODO(thakis): It'd be nice if there was a script for doing this. Maybe make
tools/fuchsa/run-swarmed.py work for win cross builds too, or create
`run_base_unittests` script targets during the build (like Android).
