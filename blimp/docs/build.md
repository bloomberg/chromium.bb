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
echo "import(\"//build/args/blimp_client.gn\")" > out-android/Debug/args.gn
gn gen out-android/Debug
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

To add your own build preferences
```bash
gn args out-android/Debug
```

## Engine

Create another out-directory and set the GN args:

```bash
mkdir -p out-linux/Debug
echo "import(\"//build/args/blimp_engine.gn\")" > out-linux/Debug/args.gn
gn gen out-linux/Debug
```

To build:

```bash
ninja -C out-linux/Debug blimp
```

To add your own build preferences
```bash
gn args out-android/Debug
```
