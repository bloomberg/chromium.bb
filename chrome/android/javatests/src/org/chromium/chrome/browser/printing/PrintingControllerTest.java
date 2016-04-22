// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.common.ContentSwitches;
import org.chromium.printing.PrintDocumentAdapterWrapper;
import org.chromium.printing.PrintManagerDelegate;
import org.chromium.printing.PrintingControllerImpl;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * Tests Android printing.
 * TODO(cimamoglu): Add a test with cancellation.
 * TODO(cimamoglu): Add a test with multiple, stacked onLayout/onWrite calls.
 * TODO(cimamoglu): Add a test which emulates Chromium failing to generate a PDF.
 */
public class PrintingControllerTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String TEMP_FILE_NAME = "temp_print";
    private static final String TEMP_FILE_EXTENSION = ".pdf";
    private static final String PRINT_JOB_NAME = "foo";
    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>foo</body></html>");
    private static final String PDF_PREAMBLE = "%PDF-1";
    private static final long TEST_TIMEOUT = 20000L;

    public PrintingControllerTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Do nothing.
    }

    private static class LayoutResultCallbackWrapperMock implements
            PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper {
        @Override
        public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {}

        @Override
        public void onLayoutFailed(CharSequence error) {}

        @Override
        public void onLayoutCancelled() {}
    }

    private static class WriteResultCallbackWrapperMock implements
            PrintDocumentAdapterWrapper.WriteResultCallbackWrapper {
        @Override
        public void onWriteFinished(PageRange[] pages) {}

        @Override
        public void onWriteFailed(CharSequence error) {}

        @Override
        public void onWriteCancelled() {}
    }

    /**
     * Test a basic printing flow by emulating the corresponding system calls to the printing
     * controller: onStart, onLayout, onWrite, onFinish.  Each one is called once, and in this
     * order, in the UI thread.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    @LargeTest
    @Feature({"Printing"})
    public void testNormalPrintingFlow() throws Throwable {
        if (!ApiCompatibilityUtils.isPrintingSupported()) return;

        startMainActivityWithURL(URL);
        final Tab currentTab = getActivity().getActivityTab();

        final PrintingControllerImpl printingController = createControllerOnUiThread();

        startControllerOnUiThread(printingController, currentTab);
        // {@link PrintDocumentAdapter#onStart} is always called first.
        callStartOnUiThread(printingController);

        // Create a temporary file to save the PDF.
        final File cacheDir = getInstrumentation().getTargetContext().getCacheDir();
        final File tempFile = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION, cacheDir);
        final ParcelFileDescriptor fileDescriptor = ParcelFileDescriptor.open(tempFile,
                (ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_READ_WRITE));

        PrintAttributes attributes = new PrintAttributes.Builder()
                .setMediaSize(PrintAttributes.MediaSize.ISO_A4)
                .setResolution(new PrintAttributes.Resolution("foo", "bar", 300, 300))
                .setMinMargins(PrintAttributes.Margins.NO_MARGINS)
                .build();

        // Use this to wait for PDF generation to complete, as it will happen asynchronously.
        final FutureTask<Boolean> result = new FutureTask<Boolean>(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return true;
            }
        });

        callLayoutOnUiThread(printingController, null, attributes,
                new LayoutResultCallbackWrapperMock() {
                    // Called on UI thread
                    @Override
                    public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                        callWriteOnUiThread(printingController, fileDescriptor, result);
                    }
                });

        FileInputStream in = null;
        try {
            // This blocks until the PDF is generated.
            result.get(TEST_TIMEOUT, TimeUnit.MILLISECONDS);
            assertTrue(tempFile.length() > 0);
            in = new FileInputStream(tempFile);
            byte[] b = new byte[PDF_PREAMBLE.length()];
            in.read(b);
            String preamble = new String(b);
            assertEquals(PDF_PREAMBLE, preamble);
        } finally {
            callFinishOnUiThread(printingController);
            if (in != null) in.close();
            // Close the descriptor, if not closed already.
            fileDescriptor.close();
            TestFileUtil.deleteFile(tempFile.getAbsolutePath());
        }

    }

    /**
     * Test for http://crbug.com/528909
     *
     * Bug: http://crbug.com/532652
     * @SmallTest
     * @Feature({"Printing"})
     */
    @CommandLineFlags.Add(
            {ContentSwitches.DISABLE_POPUP_BLOCKING, ChromeSwitches.DISABLE_DOCUMENT_MODE})
    @DisabledTest
    public void testPrintClosedWindow() throws Throwable {
        if (!ApiCompatibilityUtils.isPrintingSupported()) return;

        String html = "<html><head><title>printwindowclose</title></head><body><script>"
                + "function printClosedWindow() {"
                + "  w = window.open(); w.close();"
                + "  setTimeout(()=>{w.print(); document.title='completed'}, 0);"
                + "}</script></body></html>";

        startMainActivityWithURL("data:text/html;charset=utf-8," + html);

        Tab mTab = getActivity().getActivityTab();
        assertEquals("title does not match initial title", "printwindowclose", mTab.getTitle());

        TabTitleObserver mOnTitleUpdatedHelper = new TabTitleObserver(mTab, "completed");
        runJavaScriptCodeInCurrentTab("printClosedWindow();");
        mOnTitleUpdatedHelper.waitForTitleUpdate(5);
        assertEquals("JS did not finish running", "completed", mTab.getTitle());
    }

    private PrintingControllerImpl createControllerOnUiThread() {
        try {
            final FutureTask<PrintingControllerImpl> task =
                    new FutureTask<PrintingControllerImpl>(new Callable<PrintingControllerImpl>() {
                        @Override
                        public PrintingControllerImpl call() throws Exception {
                            return (PrintingControllerImpl) PrintingControllerImpl.create(
                                    new PrintDocumentAdapterWrapper(),
                                    PRINT_JOB_NAME);
                        }
                    });

            runTestOnUiThread(task);
            PrintingControllerImpl result = task.get(TEST_TIMEOUT, TimeUnit.MILLISECONDS);
            return result;
        } catch (Throwable e) {
            fail("Error on creating PrintingControllerImpl on the UI thread: " + e);
        }
        return null;
    }

    private void startControllerOnUiThread(final PrintingControllerImpl controller,
            final Tab tab) {
        try {
            final PrintManagerDelegate mockPrintManagerDelegate = new PrintManagerDelegate() {
                @Override
                public void print(String printJobName, PrintDocumentAdapter documentAdapter,
                        PrintAttributes attributes) {
                    // Do nothing, as we will emulate the framework call sequence within the test.
                }
            };

            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    controller.startPrint(new TabPrinter(tab), mockPrintManagerDelegate);
                }
            });
        } catch (Throwable e) {
            fail("Error on calling startPrint of PrintingControllerImpl " + e);
        }
    }

    private void callStartOnUiThread(final PrintingControllerImpl controller) {
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    controller.onStart();
                }
            });
        } catch (Throwable e) {
            fail("Error on calling onStart of PrintingControllerImpl " + e);
        }
    }

    private void callLayoutOnUiThread(
            final PrintingControllerImpl controller,
            final PrintAttributes oldAttributes,
            final PrintAttributes newAttributes,
            final PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper layoutResultCallback) {
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    controller.onLayout(
                            oldAttributes,
                            newAttributes,
                            new CancellationSignal(),
                            layoutResultCallback,
                            null);
                }
            });
        } catch (Throwable e) {
            fail("Error on calling onLayout of PrintingControllerImpl " + e);
        }
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    private void callWriteOnUiThread(
            final PrintingControllerImpl controller,
            final ParcelFileDescriptor descriptor,
            final FutureTask<Boolean> result) {
        try {
            controller.onWrite(
                    new PageRange[] {PageRange.ALL_PAGES},
                    descriptor,
                    new CancellationSignal(),
                    new WriteResultCallbackWrapperMock() {
                        @Override
                        public void onWriteFinished(PageRange[] pages) {
                            try {
                                descriptor.close();
                                // Result is ready, signal to continue.
                                result.run();
                            } catch (IOException ex) {
                                fail("Failed file operation: " + ex.toString());
                            }
                        }
                    }
            );
        } catch (Throwable e) {
            fail("Error on calling onWriteInternal of PrintingControllerImpl " + e);
        }
    }

    private void callFinishOnUiThread(final PrintingControllerImpl controller) {
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    controller.onFinish();
                }
            });
        } catch (Throwable e) {
            fail("Error on calling onFinish of PrintingControllerImpl " + e);
        }
    }
}
