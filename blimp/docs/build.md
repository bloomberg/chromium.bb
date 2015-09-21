# Using GN
Blimp only supports building using [GN](../../tools/gn/README.md), and only
supports building for Android and Linux. A quick overview over how to use GN can
be found in the GN [quick start guide](../../tools/gn/docs/quick_start.md).

## Android setup
To setup GN, run the following command:
```
gn args out-android/Debug
```
This will bring up an editor, where you can type in the following:

```
target_os = "android"
is_debug = true
is_clang = true
is_component_build = true
symbol_level = 1  # Use -g1 instead of -g2
use_goma = true
```

## Linux setup
For building for Linux, you can have a side-by-side out-directory:
```
gn args out-linux/Debug
```
Use the same arguments as above, but remove `target_os`.
```
is_debug = true
is_clang = true
is_component_build = true
symbol_level = 1  # Use -g1 instead of -g2
use_goma = true
```

# Building

To build blimp, build the target ```blimp```.

## Building for Android

```
ninja -C out-android/Debug blimp
```

## Building for Linux

```
ninja -C out-linux/Debug blimp
```
