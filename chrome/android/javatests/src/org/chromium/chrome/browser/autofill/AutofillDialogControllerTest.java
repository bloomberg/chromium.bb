// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the AutofillPopup.
 */
public class AutofillDialogControllerTest extends ChromeShellTestBase {
    private static final String SWITCH_REDUCE_SECURITY_FOR_TESTING = "reduce-security-for-testing";
    private static final long DIALOG_CALLBACK_DELAY_MILLISECONDS = 50;

    private static final String TEST_NAME = "Joe Doe";
    private static final String TEST_PHONE = "(415)413-0703";
    private static final String TEST_PHONE_UNFORMATTED = "4154130703";
    private static final String TEST_EMAIL = "email@server.com";

    private static final String TEST_CC_NUMBER = "4111111111111111";
    private static final String TEST_CC_CSC = "123";
    private static final int TEST_CC_EXP_MONTH = 11;
    private static final int TEST_CC_EXP_YEAR = 2015;

    private static final String TEST_BILLING1 = "123 Main street";
    private static final String TEST_BILLING2 = "apt 456";
    private static final String TEST_BILLING3 = "leave at the office";
    private static final String TEST_BILLING_STREET =
            TEST_BILLING1 + "\n" + TEST_BILLING2 + "\n" + TEST_BILLING3;
    private static final String TEST_BILLING_CITY = "Schenectady";
    private static final String TEST_BILLING_DL = "";  // dependent locality
    private static final String TEST_BILLING_STATE = "NY";
    private static final String TEST_BILLING_ZIP = "12345";
    private static final String TEST_BILLING_SORTING_CODE = "";  // sorting code
    private static final String TEST_BILLING_COUNTRY = "US";
    private static final String TEST_BILLING_LANGUAGE = "";  // language

    private static final String TEST_SHIPPING_NAME = "Mister Receiver";
    private static final String TEST_SHIPPING_PHONE = "+46 8 713 99 99";
    private static final String TEST_SHIPPING_PHONE_UNFORMATTED = "+4687139999";
    private static final String TEST_SHIPPING1 = "19 Farstaplan";
    private static final String TEST_SHIPPING2 = "Third floor";
    private static final String TEST_SHIPPING3 = "please call first";
    private static final String TEST_SHIPPING_STREET =
            TEST_SHIPPING1 + "\n" + TEST_SHIPPING2 + "\n" + TEST_SHIPPING3;
    private static final String TEST_SHIPPING_CITY = "Farsta";
    private static final String TEST_SHIPPING_DL = "";  // dependent locality
    private static final String TEST_SHIPPING_STATE = "Stockholm";
    private static final String TEST_SHIPPING_ZIP = "12346";
    private static final String TEST_SHIPPING_SORTING_CODE = "";  // sorting code
    private static final String TEST_SHIPPING_COUNTRY = "SE";
    private static final String TEST_SHIPPING_LANGUAGE = "";  // language

    private static final String HTML_PRELUDE = "<html>"
            + "<head>"
            + "  <meta name=\"viewport\""
            + "      content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" />"
            + "</head>"
            + "<body>"
            + "<form id=\"id-form\">"
            + "  <button id=\"id-button\">DO INTERACTIVE AUTOCOMPLETE</button>";

    private static final String HTML_POSTLUDE = "</form>"
            + "<div id=\"was-autocompleted\">no</div>"
            + ":<div id=\"autocomplete-failure-reason\"></div>"
            + "<script>"
            + "var form = document.forms[0];"
            + "form.onsubmit = function(e) {"
            + "  e.preventDefault();"
            + "  form.requestAutocomplete();"
            + "};"
            + "form.onautocomplete = function() {"
            + "  document.getElementById('was-autocompleted').textContent = 'succeeded';"
            + "};"
            + "form.onautocompleteerror = function(e) {"
            + "  document.getElementById('was-autocompleted').textContent = 'failed';"
            + "  document.getElementById('autocomplete-failure-reason').textContent = e.reason;"
            + "};"
            + "</script></body></html>";

    private static String generatePage(
            boolean requestFullBilling, boolean requestShipping, boolean requestPhoneNumbers) {
        StringBuilder sb = new StringBuilder();
        sb.append(HTML_PRELUDE);
        sb.append("<fieldset>"
                + "<input id=\"id-billing-name\" autocomplete=\"billing name\" value=\"W\">"
                + "<input id=\"id-cc-name\" autocomplete=\"cc-name\" value=\"W\">"
                + "<input id=\"id-email\" autocomplete=\"email\" type=\"email\" value=\"W@W.W\">"
                + "<input id=\"id-cc-number\" autocomplete=\"cc-number\" value=\"W\">"
                + "<input id=\"id-cc-exp\" "
                + "  autocomplete=\"cc-exp\" type=\"month\" value=\"1111-11\">"
                + "<select id=\"id-cc-exp-month\" autocomplete=\"cc-exp-month\">"
                + "  <option value=\"1\" selected>1</option>"
                + "  <option value=\"2\">2</option>"
                + "  <option value=\"3\">3</option>"
                + "  <option value=\"4\">4</option>"
                + "  <option value=\"5\">5</option>"
                + "  <option value=\"6\">6</option>"
                + "  <option value=\"7\">7</option>"
                + "  <option value=\"8\">8</option>"
                + "  <option value=\"9\">9</option>"
                + "  <option value=\"10\">10</option>"
                + "  <option value=\"11\">11</option>"
                + "  <option value=\"12\">12</option>"
                + "</select>"
                + "<select id=\"id-cc-exp-year\" autocomplete=\"cc-exp-year\">"
                + "  <option value=\"2011\" selected>2011</option>"
                + "  <option value=\"2012\">2012</option>"
                + "  <option value=\"2013\">2013</option>"
                + "  <option value=\"2014\">2014</option>"
                + "  <option value=\"2015\">2015</option>"
                + "</select>"
                + "<input id=\"id-cc-csc\" autocomplete=\"cc-csc\" value=\"W\">"
                + "<input id=\"id-cc-zip\" autocomplete=\"billing postal-code\" value=\"W\">");
        if (requestFullBilling) {
            sb.append("<input id=\"id-cc-1\" autocomplete=\"billing address-line1\" value=\"W\">"
                    + "<input id=\"id-cc-2\" autocomplete=\"billing address-line2\" value=\"W\">"
                    + "<textarea id=\"id-cc-str\""
                    + "    autocomplete=\"billing street-address\">W</textarea>"
                    + "<input id=\"id-cc-city\" autocomplete=\"billing locality\" value=\"W\">"
                    + "<input id=\"id-cc-state\" autocomplete=\"billing region\" value=\"W\">"
                    + "<select id=\"id-cc-country\" autocomplete=\"billing country\">"
                    + "  <option value=\"NL\" selected>Netherlands</option>"
                    + "  <option value=\"US\">United States</option>"
                    + "  <option value=\"SE\">Sweden</option>"
                    + "  <option value=\"RU\">Russia</option>"
                    + "</select>");
        }
        sb.append("</fieldset>");
        if (requestShipping) {
            sb.append("<fieldset>"
                    + "<input id=\"id-name\" autocomplete=\"name\" value=\"W\">"
                    + "<input id=\"id-h-name\" autocomplete=\"shipping name\" value=\"W\">"
                    + "<input id=\"id-h-1\" autocomplete=\"shipping address-line1\" value=\"W\">"
                    + "<input id=\"id-h-2\" autocomplete=\"shipping address-line2\" value=\"W\">"
                    + "<textarea id=\"id-h-str\""
                    + "    autocomplete=\"shipping street-address\">W</textarea>"
                    + "<input id=\"id-h-city\" autocomplete=\"shipping locality\" value=\"W\">"
                    + "<input id=\"id-h-state\" autocomplete=\"shipping region\" value=\"W\">"
                    + "<input id=\"id-h-zip\" autocomplete=\"shipping postal-code\" value=\"W\">"
                    + "<select id=\"id-h-country\" autocomplete=\"shipping country\">"
                    + "  <option value=\"NL\" selected>Netherlands</option>"
                    + "  <option value=\"US\">United States</option>"
                    + "  <option value=\"SE\">Sweden</option>"
                    + "  <option value=\"RU\">Russia</option>"
                    + "</select>"
                    + "</fieldset>");
        }
        if (requestPhoneNumbers) {
            sb.append("<fieldset>"
                    + "<input id=\"id-cc-tel\" autocomplete=\"billing tel\" value=\"W\">");
            if (requestShipping) {
                sb.append("<input id=\"id-h-tel\" autocomplete=\"shipping tel\" value=\"W\">"
                        + "<input id=\"id-tel\" autocomplete=\"tel\" value=\"W\">");
            }
            sb.append("</fieldset>");
        }
        sb.append(HTML_POSTLUDE);
        return UrlUtils.encodeHtmlDataUri(sb.toString());
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();
        CommandLine.init(new String[]{});
        CommandLine.getInstance().appendSwitch(SWITCH_REDUCE_SECURITY_FOR_TESTING);
    }

    @MediumTest
    @Feature({"autofill"})
    public void testFieldsAreFilledMinimal() throws InterruptedException, TimeoutException {
        final boolean requestFullBilling = false;
        final boolean requestShipping = false;
        final boolean requestPhoneNumbers = false;
        verifyFieldsAreFilled(requestFullBilling, requestShipping, requestPhoneNumbers);
    }

    @MediumTest
    @Feature({"autofill"})
    public void testFieldsAreFilledFullBilling() throws InterruptedException, TimeoutException {
        final boolean requestFullBilling = true;
        final boolean requestShipping = false;
        final boolean requestPhoneNumbers = false;
        verifyFieldsAreFilled(requestFullBilling, requestShipping, requestPhoneNumbers);
    }

    @MediumTest
    @Feature({"autofill"})
    public void testFieldsAreFilledShipping() throws InterruptedException, TimeoutException {
        final boolean requestFullBilling = true;
        final boolean requestShipping = true;
        final boolean requestPhoneNumbers = false;
        verifyFieldsAreFilled(requestFullBilling, requestShipping, requestPhoneNumbers);
    }

    @MediumTest
    @Feature({"autofill"})
    public void testFieldsAreFilledBillingPhone() throws InterruptedException, TimeoutException {
        final boolean requestFullBilling = true;
        final boolean requestShipping = false;
        final boolean requestPhoneNumbers = true;
        verifyFieldsAreFilled(requestFullBilling, requestShipping, requestPhoneNumbers);
    }

    @MediumTest
    @Feature({"autofill"})
    public void testFieldsAreFilledEverything() throws InterruptedException, TimeoutException {
        final boolean requestFullBilling = true;
        final boolean requestShipping = true;
        final boolean requestPhoneNumbers = true;
        verifyFieldsAreFilled(requestFullBilling, requestShipping, requestPhoneNumbers);
    }

    // It is currently unspecified whether autocomplete="name" gives a SHIPPING or a BILLING name.
    // I'm assuming here that this is a shipping name.
    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeName() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"name\">",
                TEST_SHIPPING_NAME, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingName() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing name\">",
                TEST_NAME, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingName() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping name\">",
                TEST_SHIPPING_NAME, "id", false, true, false);
    }

    // It is currently unspecified whether autocomplete="name" gives a SHIPPING or a BILLING phone.
    // I'm assuming here that this is a shipping phone.
    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeTel() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"tel\">",
                TEST_SHIPPING_PHONE_UNFORMATTED, "id", false, true, true);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingTel() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing tel\">",
                TEST_PHONE_UNFORMATTED, "id", true, false, true);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingTel() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping tel\">",
                TEST_SHIPPING_PHONE_UNFORMATTED, "id", false, true, true);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeEmail() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"email\" type=\"email\">",
                TEST_EMAIL, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeCCName() throws InterruptedException, TimeoutException {
        verifyOneField(
                "<input id=\"id\" autocomplete=\"cc-name\">",
                TEST_NAME, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeCCNumber() throws InterruptedException, TimeoutException {
        verifyOneField(
                "<input id=\"id\" autocomplete=\"cc-number\">",
                TEST_CC_NUMBER, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeCCCsc() throws InterruptedException, TimeoutException {
        verifyOneField(
                "<input id=\"id\" autocomplete=\"cc-csc\">",
                TEST_CC_CSC, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeCCExp() throws InterruptedException, TimeoutException {
        verifyOneField(
                "<input id=\"id\" autocomplete=\"cc-exp\" type=\"month\" value=\"1111-11\">",
                "" + TEST_CC_EXP_YEAR + "-" + TEST_CC_EXP_MONTH,
                "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeCCExpMonth() throws InterruptedException, TimeoutException {
        verifyOneField(
                "<select id=\"id\" autocomplete=\"cc-exp-month\">"
                + "  <option value=\"1\" selected>1</option>"
                + "  <option value=\"2\">2</option>"
                + "  <option value=\"3\">3</option>"
                + "  <option value=\"4\">4</option>"
                + "  <option value=\"5\">5</option>"
                + "  <option value=\"6\">6</option>"
                + "  <option value=\"7\">7</option>"
                + "  <option value=\"8\">8</option>"
                + "  <option value=\"9\">9</option>"
                + "  <option value=\"10\">10</option>"
                + "  <option value=\"11\">11</option>"
                + "  <option value=\"12\">12</option>"
                + "</select>",
                "" + TEST_CC_EXP_MONTH, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeCCExpYear() throws InterruptedException, TimeoutException {
        verifyOneField(
                "<select id=\"id\" autocomplete=\"cc-exp-year\">"
                + "  <option value=\"2011\" selected>2011</option>"
                + "  <option value=\"2012\">2012</option>"
                + "  <option value=\"2013\">2013</option>"
                + "  <option value=\"2014\">2014</option>"
                + "  <option value=\"2015\">2015</option>"
                + "</select>",
                "" + TEST_CC_EXP_YEAR, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingCountry() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<select id=\"id\" autocomplete=\"billing country\">"
                + "  <option value=\"NL\" selected>Netherlands</option>"
                + "  <option value=\"US\">United States</option>"
                + "  <option value=\"SE\">Sweden</option>"
                + "  <option value=\"RU\">Russia</option>"
                + "</select>",
                TEST_BILLING_COUNTRY, "id", true, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingPostalCode() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing postal-code\">",
                TEST_BILLING_ZIP, "id", false, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingAddressLine1() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing address-line1\">",
                TEST_BILLING1, "id", true, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingAddressLine2() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing address-line2\">",
                TEST_BILLING2, "id", true, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingStreetAddress() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<textarea id=\"id\" autocomplete=\"billing street-address\"></textarea>",
                TEST_BILLING_STREET, "id", true, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingLocality() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing locality\">",
                TEST_BILLING_CITY, "id", true, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeBillingRegion() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"billing region\">",
                TEST_BILLING_STATE, "id", true, false, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingCountry() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<select id=\"id\" autocomplete=\"shipping country\">"
                + "  <option value=\"NL\" selected>Netherlands</option>"
                + "  <option value=\"US\">United States</option>"
                + "  <option value=\"SE\">Sweden</option>"
                + "  <option value=\"RU\">Russia</option>"
                + "</select>",
                TEST_SHIPPING_COUNTRY, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingPostalCode() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping postal-code\">",
                TEST_SHIPPING_ZIP, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingAddressLine1() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping address-line1\">",
                TEST_SHIPPING1, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingAddressLine2() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping address-line2\">",
                TEST_SHIPPING2, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingStreetAddress() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<textarea id=\"id\" autocomplete=\"shipping street-address\"></textarea>",
                TEST_SHIPPING_STREET, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingLocality() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping locality\">",
                TEST_SHIPPING_CITY, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRacTypeShippingRegion() throws InterruptedException, TimeoutException {
        verifyOneFieldWithCc(
                "<input id=\"id\" autocomplete=\"shipping region\">",
                TEST_SHIPPING_STATE, "id", false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRefuseToShowWithNoCcField() throws InterruptedException, TimeoutException {
        String requested = "<input id=\"id\" autocomplete=\"shipping locality\">";
        setUpAndExpectFailedRequestAutocomplete(
                UrlUtils.encodeHtmlDataUri(HTML_PRELUDE + requested + HTML_POSTLUDE),
                false, true, false);
    }

    @SmallTest
    @Feature({"autofill"})
    public void testRefuseToShowWithNoAutocompleteAttributes()
            throws InterruptedException, TimeoutException {
        String requested = "<input id=\"id-cc-csc\">"
                + "<input id=\"id-email\" type=\"email\">"
                + "<input id=\"id-cc-name\">"
                + "<input id=\"id-shipping-locality\">";
        setUpAndExpectFailedRequestAutocomplete(
                UrlUtils.encodeHtmlDataUri(HTML_PRELUDE + requested + HTML_POSTLUDE),
                false, true, false);
    }

    private void verifyOneField(
            final String htmlFragment,
            final String expected, final String actualId,
            final boolean requestFullBilling,
            final boolean requestShipping, final boolean requestPhoneNumbers)
            throws InterruptedException, TimeoutException {
        verifyOneFieldWithOptionalCc(htmlFragment, expected, actualId,
                requestFullBilling, requestShipping, requestPhoneNumbers, false);
    }

    private void verifyOneFieldWithCc(
            final String htmlFragment,
            final String expected, final String actualId,
            final boolean requestFullBilling,
            final boolean requestShipping, final boolean requestPhoneNumbers)
            throws InterruptedException, TimeoutException {
        verifyOneFieldWithOptionalCc(htmlFragment, expected, actualId,
                requestFullBilling, requestShipping, requestPhoneNumbers, true);
    }

    private void verifyOneFieldWithOptionalCc(
            final String htmlFragment,
            final String expected, final String actualId,
            final boolean requestFullBilling,
            final boolean requestShipping, final boolean requestPhoneNumbers,
            final boolean requestCcInfo)
            throws InterruptedException, TimeoutException {
        final String optionalCcFragment = requestCcInfo
                ? "<input id=\"id-opt-cc-csc\" autocomplete=\"cc-csc\">"
                : "";
        final String url = UrlUtils.encodeHtmlDataUri(
                HTML_PRELUDE
                + htmlFragment
                + optionalCcFragment
                + HTML_POSTLUDE);

        setUpAndRequestAutocomplete(url, requestFullBilling, requestShipping, requestPhoneNumbers);

        final ContentViewCore viewCore = getActivity().getActiveContentViewCore();

        assertEquals(actualId + " did not match",
                expected, DOMUtils.getNodeValue(viewCore, actualId));
        if (requestCcInfo) {
            assertEquals("cc-csc did not match",
                    TEST_CC_CSC, DOMUtils.getNodeValue(viewCore, "id-opt-cc-csc"));
        }
    }

    private void verifyFieldsAreFilled(final boolean requestFullBilling,
            final boolean requestShipping, final boolean requestPhoneNumbers)
            throws InterruptedException, TimeoutException {
        setUpAndRequestAutocomplete(
                generatePage(requestFullBilling, requestShipping, requestPhoneNumbers),
                requestFullBilling, requestShipping, requestPhoneNumbers);

        final ContentViewCore viewCore = getActivity().getActiveContentViewCore();

        assertEquals("billing name did not match",
                TEST_NAME, DOMUtils.getNodeValue(viewCore, "id-billing-name"));
        assertEquals("email did not match",
                TEST_EMAIL, DOMUtils.getNodeValue(viewCore, "id-email"));

        assertEquals("cc-name did not match",
                TEST_NAME, DOMUtils.getNodeValue(viewCore, "id-cc-name"));

        assertEquals("cc-number did not match",
                TEST_CC_NUMBER, DOMUtils.getNodeValue(viewCore, "id-cc-number"));
        assertEquals("cc-csc did not match",
                TEST_CC_CSC, DOMUtils.getNodeValue(viewCore, "id-cc-csc"));

        assertEquals("cc-exp did not match",
                "" + TEST_CC_EXP_YEAR + "-" + TEST_CC_EXP_MONTH,
                DOMUtils.getNodeValue(viewCore, "id-cc-exp"));

        assertEquals("cc-exp-month did not match",
                "" + TEST_CC_EXP_MONTH,
                DOMUtils.getNodeValue(viewCore, "id-cc-exp-month"));
        assertEquals("cc-exp-year did not match",
                "" + TEST_CC_EXP_YEAR,
                DOMUtils.getNodeValue(viewCore, "id-cc-exp-year"));

        assertEquals("billing postal-code did not match",
                TEST_BILLING_ZIP, DOMUtils.getNodeValue(viewCore, "id-cc-zip"));

        if (requestFullBilling) {
            assertEquals("billing address-line1 did not match",
                    TEST_BILLING1, DOMUtils.getNodeValue(viewCore, "id-cc-1"));
            assertEquals("billing address-line2 did not match",
                    TEST_BILLING2, DOMUtils.getNodeValue(viewCore, "id-cc-2"));
            assertEquals("billing street-address did not match",
                    TEST_BILLING_STREET, DOMUtils.getNodeValue(viewCore, "id-cc-str"));
            assertEquals("billing locality did not match",
                    TEST_BILLING_CITY, DOMUtils.getNodeValue(viewCore, "id-cc-city"));
            assertEquals("billing region did not match",
                    TEST_BILLING_STATE, DOMUtils.getNodeValue(viewCore, "id-cc-state"));
            assertEquals("billing country did not match",
                    TEST_BILLING_COUNTRY, DOMUtils.getNodeValue(viewCore, "id-cc-country"));

            if (requestPhoneNumbers) {
                assertEquals("billing tel did not match",
                        TEST_PHONE_UNFORMATTED,
                        DOMUtils.getNodeValue(viewCore, "id-cc-tel"));
            }
        }

        if (requestShipping) {
            assertEquals("shipping name did not match",
                    TEST_SHIPPING_NAME, DOMUtils.getNodeValue(viewCore, "id-h-name"));
            assertEquals("shipping postal-code did not match",
                    TEST_SHIPPING_ZIP, DOMUtils.getNodeValue(viewCore, "id-h-zip"));
            assertEquals("shipping address-line1 did not match",
                    TEST_SHIPPING1, DOMUtils.getNodeValue(viewCore, "id-h-1"));
            assertEquals("shipping address-line2 did not match",
                    TEST_SHIPPING2, DOMUtils.getNodeValue(viewCore, "id-h-2"));
            assertEquals("shipping street-address did not match",
                    TEST_SHIPPING_STREET, DOMUtils.getNodeValue(viewCore, "id-h-str"));
            assertEquals("shipping locality did not match",
                    TEST_SHIPPING_CITY, DOMUtils.getNodeValue(viewCore, "id-h-city"));
            assertEquals("shipping region did not match",
                    TEST_SHIPPING_STATE, DOMUtils.getNodeValue(viewCore, "id-h-state"));
            assertEquals("shipping country did not match",
                    TEST_SHIPPING_COUNTRY,
                    DOMUtils.getNodeValue(viewCore, "id-h-country"));

            // It is currently unspecified whether autocomplete="name" gives a SHIPPING or
            // a BILLING name. I'm assuming here that this is a shipping name.
            assertEquals("name did not match",
                    TEST_SHIPPING_NAME, DOMUtils.getNodeValue(viewCore, "id-name"));

            if (requestPhoneNumbers) {
                assertEquals("shipping tel did not match",
                        TEST_SHIPPING_PHONE_UNFORMATTED,
                        DOMUtils.getNodeValue(viewCore, "id-h-tel"));

                // It is currently unspecified whether autocomplete="name" gives a SHIPPING or
                // a BILLING phone. I'm assuming here that this is a shipping phone.
                assertEquals("tel did not match",
                        TEST_SHIPPING_PHONE_UNFORMATTED,
                        DOMUtils.getNodeValue(viewCore, "id-tel"));
            }
        }
    }

    // Wait and assert helper methods -------------------------------------------------------------

    private void setUpAndRequestAutocomplete(final String url,
            final boolean requestFullBilling,
            final boolean requestShipping,
            final boolean requestPhoneNumbers) throws InterruptedException, TimeoutException {
        setUpAndRequestAutocompleteImpl(url,
                requestFullBilling, requestShipping, requestPhoneNumbers,
                false);
    }

    private void setUpAndExpectFailedRequestAutocomplete(final String url,
            final boolean requestFullBilling,
            final boolean requestShipping,
            final boolean requestPhoneNumbers) throws InterruptedException, TimeoutException {
        setUpAndRequestAutocompleteImpl(url,
                requestFullBilling, requestShipping, requestPhoneNumbers,
                true);
    }

    private void setUpAndRequestAutocompleteImpl(final String url,
            final boolean requestFullBilling,
            final boolean requestShipping,
            final boolean requestPhoneNumbers,
            final boolean expectFailure)
            throws InterruptedException, TimeoutException {
        launchChromeShellWithUrl(url);
        assertTrue(waitForActiveShellToBeDoneLoading());

        final ContentViewCore viewCore = getActivity().getActiveContentViewCore();

        AutofillDialogResult.ResultWallet result = new AutofillDialogResult.ResultWallet(
                TEST_EMAIL, "Google Transaction ID",
                new AutofillDialogResult.ResultCard(
                        TEST_CC_EXP_MONTH, TEST_CC_EXP_YEAR,
                        TEST_CC_NUMBER, TEST_CC_CSC),
                new AutofillDialogResult.ResultAddress(
                        TEST_NAME, TEST_PHONE,
                        TEST_BILLING_STREET,
                        TEST_BILLING_CITY, TEST_BILLING_DL, TEST_BILLING_STATE,
                        TEST_BILLING_ZIP, TEST_BILLING_SORTING_CODE, TEST_BILLING_COUNTRY,
                        TEST_BILLING_LANGUAGE),
                new AutofillDialogResult.ResultAddress(
                        TEST_SHIPPING_NAME, TEST_SHIPPING_PHONE,
                        TEST_SHIPPING_STREET,
                        TEST_SHIPPING_CITY, TEST_SHIPPING_DL, TEST_SHIPPING_STATE,
                        TEST_SHIPPING_ZIP, TEST_SHIPPING_SORTING_CODE, TEST_SHIPPING_COUNTRY,
                        TEST_SHIPPING_LANGUAGE));
        MockAutofillDialogController.installMockFactory(
                DIALOG_CALLBACK_DELAY_MILLISECONDS,
                result,
                true, "", "", "", "",
                requestFullBilling, requestShipping, requestPhoneNumbers);

        DOMUtils.clickNode(this, viewCore, "id-button");
        waitForInputFieldFill(viewCore);

        if (!expectFailure) {
            assertEquals("requestAutocomplete failed",
                    "succeeded",
                    DOMUtils.getNodeContents(viewCore, "was-autocompleted"));
        } else {
            assertEquals("requestAutocomplete succeeded when it should be failing",
                    "failed",
                    DOMUtils.getNodeContents(viewCore, "was-autocompleted"));
        }
    }

    private void waitForInputFieldFill(final ContentViewCore viewCore) throws InterruptedException {
        assertTrue("requestAutocomplete() never completed.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        String wasAutocompleted;
                        try {
                            wasAutocompleted = DOMUtils.getNodeContents(
                                    viewCore, "was-autocompleted");
                        } catch (InterruptedException e) {
                            return false;
                        } catch (TimeoutException e) {
                            return false;
                        }
                        return TextUtils.equals("succeeded", wasAutocompleted)
                                || TextUtils.equals("failed", wasAutocompleted);
                    }
                }));
    }
}
