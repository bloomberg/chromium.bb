# Build API

Welcome to the Build API.

### proto/
This directory contains all of the raw .proto files.
You will find message, service, and method definitions, and their configurations.

* build_api.proto contains service and method option definitions.
* common.proto contains well shared messages.

### gen/
The generated protobuf messages.

**Do not edit files in this package directly!**

The proto can be compiled using the `compile_build_api_proto` script in the api directory.
Compiling the proto is currently only confirmed working on 3.6.1, but definitely requires
greater than 3.3.0, and protoc must be installed locally.
This will not require a specific local version as soon as the chroot protobuf upgrade is complete.


### service/

This directory contains all of the implemented services.
The protobuf service module option (defined in build_api.proto) references the module in this
package that implements the service.
