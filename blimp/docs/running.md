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

### In a container
For running the engine in a container, see [container](container.md).

### On a workstation
If you are running the engine on your workstation and are connected to the
client device via USB, you'll need remote port forwarding to allow the Blimp
client to talk to your computer. Follow the instructions
[here](https://developer.chrome.com/devtools/docs/remote-debugging) to get
started. You'll probably want to remap 25467 to "localhost:25467".

### Required flags
*   `--blimp-client-token-path=$PATH`: Path to a file containing a nonempty
token string. If this is not present, the engine will fail to boot.
