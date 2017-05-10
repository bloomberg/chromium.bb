# Bluetooth Testing

Implementation of the Bluetooth component is tested via unittests. Client code
uses Mock Bluetooth objects:


## Mojo Testing Interface Implementation

See [bluetooth/public/interfaces/test](/device/bluetooth/public/interfaces/test)
for details about the interface. The current implementation of this interface
creates a fake implementation of the current non-mojo C++ Bluetooth interface.
This interface is implemented across files with a "fake_" prefix.
*This interface may be removed when a Bluetooth Mojo Service is introduced, if
Web Bluetooth remains its only client. Testing code would implement the service
as needed for tests.*


## Cross Platform Unit Tests

New feature development uses cross platform unit tests. This reduces test code
redundancy and produces consistency across all implementations.

Unit tests operate at the public `device/bluetooth` API layer and the
`BluetoothTest` fixture controls fake operating system behavior as close to the
platfom as possible. The resulting test coverage spans the cross platform API,
common implementation, and platform specific implementation as close to
operating system APIs as possible.

`test/bluetooth_test.h` defines the cross platform test fixture
`BluetoothTestBase`. Platform implementations provide subclasses, such as
`test/bluetooth_test_android.h` and typedef to the name `BluetoothTest`.

[More testing information](https://docs.google.com/document/d/1mBipxn1sJu6jMqP0RQZpkYXC1o601bzLCpCxwTA2yGA/edit?usp=sharing)


## Legacy Platform Specific Unit Tests

Early code (Classic on most platforms, and Low Energy on BlueZ) was tested with
platform specific unit tests, e.g. `bluetooth_bluez_unittest.cc` &
`bluetooth_adapter_win_unittest.cc`. The BlueZ style has platform specific
methods to create fake devices and the public API is used to interact with them.

Maintenance of these earlier implementation featuress should update tests in
place. Long term these tests should be [refactored into cross platform
tests](https://crbug.com/580403).


## Mock Bluetooth Objects

`test/mock_bluetooth_*` files provide GoogleMock based fake objects for use in
client code.


## Chrome OS Blueooth Controller Tests

Bluetooth controller system tests generating radio signals are run and managed
by the Chrome OS team. See:
https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/server/site_tests/
https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/server/cros/bluetooth/
https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/client/cros/bluetooth/
