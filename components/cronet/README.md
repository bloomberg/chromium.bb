# Quick Start Guide to Using Cronet
Cronet is the networking stack of Chromium put into a library for use on
mobile. This is the same networking stack that is used in the Chrome browser
by over a billion people. It offers an easy-to-use, high performance,
standards-compliant, and secure way to perform HTTP requests. Cronet has support
for both Android and iOS. On Android, Cronet offers its own Java asynchronous
API as well as support for the [java.net.HttpURLConnection] API.
This document gives a brief introduction to using these two Java APIs.

### Basics
First you will need to implement the `UrlRequestListener` interface to handle
events during the lifetime of a request. For example:

    class MyListener implements UrlRequestListener {
        @Override
        public void onReceivedRedirect(UrlRequest request,
                ResponseInfo responseInfo, String newLocationUrl) {
            if (followRedirect) {
                // Let's tell Cronet to follow the redirect!
                mRequest.followRedirect();
            } else {
                // Not worth following the redirect? Abandon the request.
                mRequest.cancel();
            }
        }

        @Override
        public void onResponseStarted(UrlRequest request,
                ResponseInfo responseInfo) {
             // Now we have response headers!
             int httpStatusCode = responseInfo.getHttpStatusCode();
             if (httpStatusCode == 200) {
                 // Success! Let's tell Cronet to read the response body.
                 request.read(myBuffer);
             } else if (httpStatusCode == 503) {
                 // Do something. Note that 4XX and 5XX are not considered
                 // errors from Cronet's perspective since the response is
                 // successfully read.
             }
             responseHeaders = responseInfo.getAllHeaders();
        }

        @Override
        public void onReadCompleted(UrlRequest request,
                ResponseInfo responseInfo, ByteBuffer byteBuffer) {
             // Response body is available.
             doSomethingWithResponseData(byteBuffer);
             // Let's tell Cronet to continue reading the response body or
             // inform us that the response is complete!
             request.read(myBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request,
                ExtendedResponseInfo extendedResponseInfo) {
             // Request has completed successfully!
        }

        @Override
        public void onFailed(UrlRequest request,
                ResponseInfo responseInfo, UrlRequestException error) {
             // Request has failed. responseInfo might be null.
             Log.e("MyListener", "Request failed. " + error.getMessage());
             // Maybe handle error here. Typical errors include hostname
             // not resolved, connection to server refused, etc.
        }
    }

Make a request like this:

    UrlRequestContextConfig myConfig = new UrlRequestContextConfig();
    CronetUrlRequestContext myRequestContext =
            new CronetUrlRequestContext(getContext(), myConfig);
    Executor executor = ExecutorService.newSingleThreadExecutor();
    MyListener listener = new MyListener();
    UrlRequest request = myRequestContext.createRequest(
            "https://www.example.com", listener, executor);
    request.start();

In the above example, `MyListener` implements the `UrlRequestListener`
interface. The request is started asynchronously. When the response is ready
(fully or partially), and in the event of failures or redirects,
`listener`'s methods will be invoked on `executor`'s thread to inform the
client of the request state and/or response information.

### Downloading Data
When Cronet fetches response headers from the server or gets them from the
cache, `UrlRequestListener.onResponseStarted` will be invoked. To read the
response body, the client should call `UrlRequest.read` and supply a
[ByteBuffer] for Cronet to fill. Once a portion or all of
the response body is read, `UrlRequestListener.onReadCompleted` will be invoked.
The client may consume the data, or copy the contents of the `byteBuffer`
elsewhere for use later. The data in `byteBuffer` is only guaranteed to be
valid for the duration of the `UrlRequestListener.onReadCompleted` callback.
Once the client is ready to consume more data, the client should call
`UrlRequest.read` again. The process continues until
`UrlRequestListener.onSucceeded` or `UrlRequestListener.onFailed` is invoked,
which signals the completion of the request.

### Uploading Data
    MyUploadDataProvider myUploadDataProvider = new MyUploadDataProvider();
    request.setHttpMethod("POST");
    request.setUploadDataProvider(myUploadDataProvider, executor);
    request.start();

In the above example, `MyUploadDataProvider` implements the
`UploadDataProvider` interface. When Cronet is ready to send the request body,
`myUploadDataProvider.read(UploadDataSink uploadDataSink,
ByteBuffer byteBuffer)` will be invoked. The client will need to write the
request body into `byteBuffer`. Once the client is done writing into
`byteBuffer`, the client can let Cronet know by calling
`uploadDataSink.onReadSucceeded`. If the request body doesn't fit into
`byteBuffer`, the client can continue writing when `UploadDataProvider.read` is
invoked again. For more details, please see the API reference.

### <a id=configuring-cronet></a> Configuring Cronet
Various configuration options are available via the `UrlRequestContextConfig`
object.

Enabling HTTP/2, QUIC, or SDCH:

- For Example:

        myConfig.enableSPDY(true).enableQUIC(true).enableSDCH(true);

Controlling the cache:

- Use a 100KiB in-memory cache:

        myConfig.enableHttpCache(
                UrlRequestContextConfig.HttpCache.IN_MEMORY, 100 * 1024);

- or use a 1MiB disk cache:

        myConfig.setStoragePath(storagePathString);
        myConfig.enableHttpCache(UrlRequestContextConfig.HttpCache.DISK,
                1024 * 1024);

### Debugging
To get more information about how Cronet is processing network
requests, you can start and stop **NetLog** logging by calling
`UrlRequestContext.startNetLogToFile` and `UrlRequestContext.stopNetLog`.
Bear in mind that logs may contain sensitive data. You may analyze the
generated log by navigating to [chrome://net-internals#import] using a
Chrome browser.

# Using the java.net.HttpURLConnection API
Cronet offers an implementation of the [java.net.HttpURLConnection] API to make
it easier for apps which rely on this API to use Cronet.
To use Cronet's implementation instead of the system's default implementation,
simply do the following:

    CronetURLStreamHandlerFactory streamHandlerFactory =
            new CronetURLStreamHandlerFactory(getContext(), myConfig);
    URL.setURLStreamHandlerFactory(streamHandlerFactory);

Cronet's
HttpURLConnection implementation has some limitations as compared to the system
implementation, including not utilizing the default system HTTP cache (Please
see {@link org.chromium.net.urlconnection.CronetURLStreamHandlerFactory} for
more information).
You can configure Cronet and control caching through the
`UrlRequestContextConfig` instance, `myConfig`
(See [Configuring Cronet](#configuring-cronet) section), before you pass it
into the `CronetURLStreamHandlerFactory` constructor.

[ByteBuffer]: https://developer.android.com/reference/java/nio/ByteBuffer.html
[chrome://net-internals#import]: chrome://net-internals#import
[java.net.HttpURLConnection]: https://developer.android.com/reference/java/net/HttpURLConnection.html
