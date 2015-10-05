# Running Blimp

See [build](build.md) for instructions on how to build Blimp.

## Running the client

There are both Android and Linux clients.  The Android client is the shipping
client while the Linux client is used for development purposes.

### Android Client

Install the Blimp APK with the following:

```bash
./build/android/adb_install_apk.py $(PRODUCT_DIR)/apks/Blimp.apk
```

Set up any command line flags with:

```bash
./build/android/adb_blimp_command_line --enable-webgl
```

Run the Blimp APK with:

```bash
adb_run_blimp_client
```

### Linux Client

TBD

## Running the engine

For running the engine in a container, see [container](container.md).
