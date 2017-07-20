# Publishing a new Fuchsia SDK, and updating Chrome to use it

Updating the Fuchsia SDK in Chromium currently involves:
1. Building and testing the latest Fuchsia version.
0. Packaging the SDK from that and uploading to cloud storage.
0. Updating Chromium with DEPS and any other changes to build with the new SDK.

## Build and test Fuchsia

For current documentation on Fuchsia setup, visit the [Fuchsia Getting Started Guide](https://fuchsia.googlesource.com/docs/+/HEAD/getting_started.md).

Perform these steps on your Linux workstation:
1. Check if fuchsia-dashboard.appspot.com looks green.
0. Fetch the latest Fuchsia source.

        $ jiri update

0. Build Magenta, the sysroot, and Fuchsia.

        $ fbuild

Now verify that the build is stable:

1. Either: Boot the build in QEMU - you will probably want to use -k to enable KVM acceleration (see the guide for details of other options to enable networking, graphics, etc).

        $ frun -k

0. Or (preferably): Boot the build on hardware (see the guide for details of first-time setup).

        $ fboot

0. Run tests to verify the build. All the tests should pass! If they don't then find out which of the test fixtures is failing and ping #cr-fuchsia on IRC to determine if it's a known issue. 

        $ runtests

## Build and upload the SDK
1. Configure and build a release mode build with sdk config:

        $ ./packages/gn/gen.py --release --goma -m runtime
        $ ./packages/gn/build.py --release -j1000

0. Package the sysroot and tools.

        $ ./scripts/makesdk.go .

0. Compute the SHA-1 hash of the tarball, to use as its filename.

        $ sha1sum fuchsia-sdk.tgz
        f79f55be4e69ebd90ea84f79d7322525853256c3

0. Note that it's possible to un-tar the SDK into place in your Chromium checkout at this point, for testing, and skip to the Build & Test steps below, before doing the actual upload.

0. (Googlers only) Upload the tarball to the fuchsia-build build, under the SDK path. This will require "create" access to the bucket, which is restricted to Googlers, and you'll need to have authenticated to the gcloud tools with OAuth.

        $ gsutil.py cp fuchsia-sdk.tgz gs://fuchsia-build/fuchsia/sdk/linux64/f79f55be4e69ebd90ea84f79d7322525853256c

## Update & test Chromium

1. Update the fuchsia SDK line in top-level DEPS with the new hash.

0. Fetch the new SDK.

        $ gclient sync

0. Build & test.

        $ ninja -C out/gnRelease ... <check build-bots for current targets> ...

0. Make necessary changes to fix build, to include in the roll CL.  You may find problems in dependencies of Chromium, in which case you'll need to fix those, get the dependency rolled, and then try again.

0. Upload the roll CL for review!  Remember to run the Fuchsia try-bots over the patch-set before landing it.
