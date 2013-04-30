This directory contains an example of chrome application that uses native
messaging API that allows to communicate with a native application.

In order for this example to work you must first install the native messaging
host from the host directory. To install the host on Windows:
  1. Replace HOST_PATH in host/com.google.chrome.example.echo.json with the full
     path to host/native-messaging-example-host.bat
  2. In the registry key HKEY_LOCAL_MACHINE\SOFTWARE\Google\Chrome\NativeMessagingHosts
     add string value "com.google.chrome.example.echo" with full path to
     host/com.google.chrome.example.echo.json .

Note that you need to have python installed.

On Mac and Linux you can use install_host.sh script in the host directory:
  sudo host/install_host.sh
You can later use host/uninstall_host.sh to uninstall the host.
