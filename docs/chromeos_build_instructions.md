# ChromeOS Build Instructions (Chromium OS on Linux)

Chromium on Chromium OS is built on a mix of code sourced from Chromium
on Linux and Chromium on Windows. Much of the user interface code is
shared with Chromium on Windows. As such, if you make a change to
Chromium on Windows you may find your changes affect Chromium on
Chromium OS. Fortunately to test the effect of your changes you don't
have to build all of Chromium OS, you can just build Chromium for
Chromium OS directly on Linux.

First, follow the [normal Linux build
instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md)
as usual to get a Chromium checkout.

## Running Chromium on your local machine

If you plan to test the Chromium build on your dev machine and not a
Chromium OS device run:

```shell
export GYP_DEFINES="chromeos=1"
gclient runhooks
```

Now, once you build, you will build with Chromium OS features turned on.

### Notes

When you build Chromium OS Chromium, you'll be using the TOOLKIT\_VIEWS
front-end just like Windows, so the files you'll probably want are in
src/ui/views and src/chrome/browser/ui/views.

If chromeos=1 is specified, then toolkit\_views=0 must not be specified.

The Chromium OS build requires a functioning GL so if you plan on
testing it through Chromium Remote Desktop you might face drawing
problems (e.g. Aura window not painting anything). Possible remedies:

*   --ui-enable-software-compositing --ui-disable-threaded-compositing
*   --use-gl=osmesa, but it's ultra slow, and you'll have to build
    osmesa yourself.
*   ... or just don't use Remote Desktop. :)

Note the underscore in the GYP_DEFINES variable name, as people
sometimes mistakenly write it GYPDEFINES.

To more closely match the UI used on devices, you can install fonts used
by Chrome OS, such as Roboto, on your Linux distro.

To specify a logged in user:

*   For first run, add the following options to the command line:
    **--user-data-dir=/tmp/chrome --login-manager**
*   Go through the out-of-the-box UX and sign in as
    **username@gmail.com**
*   For subsequent runs, add the following to the command line:
    **--user-data-dir=/tmp/chrome --login-user=username@gmail.com**.
*   To run in guest mode instantly, you can run add the arguments
    **--user-data-dir=/tmp/chrome --bwsi --incognito
    --login-user='$guest' --login-profile=user**

Signing in as a specific user is useful for debugging features like sync
that require a logged in user.

### Compile Testing Chromium with the Chromium OS SDK (quick version)

Note: These instructions are intended for Chromium developers trying to
diagnose compile issues on Chromium OS, which can block changes in the
CQ. See the [full
documentation](http://www.chromium.org/chromium-os/how-tos-and-troubleshooting/building-chromium-browser)
for more information about building & testing chromium for Chromium OS.

To do a build of Chromium that can run on Chromium OS itself, the Chromium OS
SDK must be used. The SDK provides all of chromium's dependencies as they are
distributed with Chromium OS (as opposed to other distributions such as Ubuntu).

To enter the SDK build environment, run the following command (replace the value
of the `--board` flag with the name of the configuration you want to test).

```shell
cros chrome-sdk --board=amd64-generic --use-external-config
```

Once in the SDK build environment, build using the normal linux workflow (except
for a different build directory):

```shell
gclient runhooks
ninja -C out_amd64-generic/Release chromium_builder_tests
```

The current configurations verified by the CQ are:

  Board Flag | Build Directory | CPU architecture
  --- | --- | ---
  amd64-generic | out_amd64-generic | 64-bit Intel
  x86-generic | out_x86-generic | 32-bit Intel
  daisy | out_daisy | 32-bit ARM

## Running Chromium on a Chromium OS device

Look at the [Chromium OS
documentation](http://www.chromium.org/chromium-os/how-tos-and-troubleshooting/building-chromium-browser)
for the official flow for doing this.
