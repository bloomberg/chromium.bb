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

## Building and running Chromium with Chromium OS UI on your local machine

If you plan to test the Chromium build on your dev machine and not a
Chromium OS device, run the following in your chromium checkout:

    $ gn gen out/Default --args='target_os="chromeos"'
    $ ninja -C out/Default

NOTE: You may wish to replace 'Default' with something like 'Cros' if
you switch back and forth between Linux and Chromium OS builds, or 'Debug'
if you want to differentiate between Debug and Release builds (see below)
or DebugCros or whatever you like.

Now, when you build, you will build with Chromium OS features turned on.

See [GN Build Configuration](https://www.chromium.org/developers/gn-build-configuration)
for more information about configuring your build.

Some additional options you may wish to set:
```
    is_component_build = true
    use_goma = true
    is_debug = false  # Release build
    dcheck_always_on = true  # Enable DCHECK (with is_debug = false)
    is_official_build = true
    is_chrome_branded = true
```

## Notes

When you build Chromium OS Chromium, you'll be using the TOOLKIT\_VIEWS
front-end just like Windows, so the files you'll probably want are in
src/ui/views and src/chrome/browser/ui/views.

When target_os = "chromeos", then toolkit\_views need not (and should not)
be specified.

The Chromium OS build requires a functioning GL so if you plan on
testing it through Chromium Remote Desktop you might face drawing
problems (e.g. Aura window not painting anything). Possible remedies:

*   --ui-enable-software-compositing --ui-disable-threaded-compositing
*   --use-gl=osmesa, but it's ultra slow, and you'll have to build
    osmesa yourself.
*   ... or just don't use Remote Desktop. :)

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

## Compile Testing Chromium with the Chromium OS SDK (quick version)

See [Building Chromium for a Chromium OS device](https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/building-chromium-browser)
for information about building & testing chromium for Chromium OS.
