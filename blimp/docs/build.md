# Using GN
Blimp only supports building using [GN](../../tools/gn/README.md). A quick
overview over how to use GN can be found in the GN
[quick start guide](../../tools/gn/docs/quick_start.md).

There are three different build configurations depending on what you want to
build:

## Android client

Create an out-directory and set the GN args:

```bash
mkdir -p out-android/Debug
gn args out-android/Debug
```

This will bring up an editor, where you can type in the following:

```bash
target_os = "android"
is_debug = true
is_clang = true
is_component_build = true
symbol_level = 1  # Use -g1 instead of -g2
use_goma = true
```

To build:

```bash
ninja -C out-android/Debug blimp
```

You can also build and install incremental APK like this:

```bash
ninja -C out-android/Debug blimp blimp_apk_incremental &&
    out-android/Debug/bin/install_blimp_apk_incremental
```

## Engine inside a Docker container

Create another out-directory and set the GN args. Note, when building to run
inside a Docker container you'll need to set the target_os to "chromeos":

```bash
mkdir -p out-chromeos/Debug
gn args out-chromeos/Debug
```

This will bring an editor, where you can type in the following:

```bash
target_os = "chromeos"
is_debug = true
is_clang = true
symbol_level = 1  # Use -g1 instead of -g2
use_goma = true
use_aura = true
use_ozone = true
use_alsa = false
use_pulseaudio = false
```

To build:

```bash
ninja -C out-chromeos/Debug blimp
```

## "Bare" engine, no Docker container

Create another out-directory and set the GN args:

```bash
mkdir -p out-linux/Debug
gn args out-linux/Debug
```

This will bring an editor, where you can type in the following:

```bash
is_debug = true
is_clang = true
is_component_build = true
symbol_level = 1  # Use -g1 instead of -g2
use_goma = true
use_aura = true
use_ozone = true
```

To build:

```bash
ninja -C out-linux/Debug blimp
```
