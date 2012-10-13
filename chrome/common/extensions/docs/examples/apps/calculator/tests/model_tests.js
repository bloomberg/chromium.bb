/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 **/

window.onload = function() {

  // TODO(dharcourt@chromium.org): Organize and beef up these tests.
  // TODO(dharcourt@chromium.org): testModel("*", '~ = [-0]');
  // TODO(dharcourt@chromium.org): testModel("*", '~42 [[-42]]');
  // TODO(dharcourt@chromium.org): testModel("*", '1 / 3 * 3 = [[1]]');
  // TODO(dharcourt@chromium.org): Test {nega,posi}tive {under,over}flows.

  test("Initialization", function() {
    var model = new Model();
    equal(model.accumulator, null);
    equal(model.operator, null);
    equal(model.operand, null);
    equal(model.defaults.operator, null);
    equal(model.defaults.operand, null);
  });

  testModel("AC", '1 + 2 = [3] 4 A [null null null]');

  testModel("back", '1 + 2 < [1 + null] < [1 null null] < [1 null null]');
  // TODO(dharcourt@chromium.org): Test more AC, C, back

  testModel("Miscellaneous Test A",
            '2 [[2]] + [2 + null] = [4  null null]' +
            '        + [4 + null] = [8  null null]' +
            '                     = [12 null null]');

  testModel("Miscellaneous Test B", '2 * [2] = [4] * = [16] = [64]');

  testModel("Miscellaneous Test C", '0.020202020 [[0.02020202=]]');

  testModel("Miscellaneous Test D", '.2 [[0 .2]]');

  testModel("Miscellaneous Test E", '0.000000014 [[0.00000001=]]');

  testModel("Miscellaneous Test F", '0.100000004 [[0.10000000=]]');

  testModel("Miscellaneous Test G", '0.123123124 [[0.12312312=]]');

  testModel("Miscellaneous Test H", '1.231231234 [[1.23123123=]]');

  testModel("Miscellaneous Test I", '123.1231234 [[123.123123=]]');

  testModel("Miscellaneous Test J", '123123.1234 [[123123.123=]]');

  testModel("Miscellaneous Test K", '12312312.34 [[12312312.3=]]');

  testModel("Miscellaneous Test L", '12312312.04 [[12312312.0=]]');

  testModel("Miscellaneous Test M", '1231231234 [[123123123=]]');

  testModel("Miscellaneous Test N", '1 + 1 + [2] = [4]');

  testModel("Miscellaneous Test O", '1 + 12 [1 + [12]]');

  testModel("Positive + Positive", '82959 + 4 = [82963]');

  testModel("Positive + Negative", '123 + 456~ = [-333]');

  testModel("Negative + Positive", '502~ + 385 = [-117]');

  testModel("Negative + Negative", '4296~ + 32~ = [-4328]');

  testModel("Positive + Zero", '23650 + 0 = [23650]');

  testModel("Negative + Zero", '489719~ + 0 = [-489719]');

  testModel("Zero + Positive", '0 + 4296 = [4296]');

  testModel("Zero + Negative", '0 + 322~ = [-322]');

  testModel("Zero + Zero", '0 + 0 = [0]');

  testModel("Addition Chain",
            '   + [0] 5 + [5] 3~ + [2] 2~ + [0] 1~ + [-1] 3 + [2] 0 = [2]');

  testModel("Positive - Positive", '4534 - 327 = [4207]');

  testModel("Subtraction Chain",
            '- [0] 5 - [-5] 3~ - [-2] 2~ - [0] 1~ - [1] 3 - [-2] 0 = [-2]');

  testModel("Positive * Positive", '7459 * 660 = [4922940]');

  testModel("Multiplication Chain",
            '* [0] 5 = [0] 1 * [1] 5 * [5] 2~ ' +
            '* [-10] 1.5 * [-15] 1~ * [15] 0 = [0]');

  testModel("Positive / Positive", '181 / 778 = [0.23264781]');

  testModel("Division Chain",
            '/ [0] 5 = [0] 1 / [1] 5 / [0.2] 2~ ' +
            '/ [-0.1] 0.1 / [-1] 1~ / [1] 0 = [E]');

  testModel("Positive Decimal Plus Positive Decimal",
            '7.48 + 8.2 = [15.68]');

  testModel("Decimals with Leading Zeros",
            '0.0023 + 0.082 = [0.0843]');

  testModel("Addition and Subtraction Chain",
            '4 + [4] 1055226 - [1055230] 198067 = [857163]');

  testModel("Multiplication and Division Chain",
            '580 * [580] 544 / [315520] 64 = [4930]');

  testModel("Addition After Equals",
            '5138 + 3351301 = [3356439] 550  + 62338 = [62888]');

  testModel("Missing First Operand in Addition", '+ 9701389 = [9701389]');

  testModel("Missing First Operand in Subtraction", '- 1770 = [-1770]');

  testModel("Missing First Operand in Multiplication", '* 65146 = [0]');

  testModel("Missing First Operand in Division", '/ 8 = [0]');

  testModel("Missing Second Operand in Addition", '28637 + = [57274]');

  testModel("Missing Second Operand in Subtraction", '52288 - = [0]');

  testModel("Missing Second Operand in Multiplication", '17 * = [289]');

  testModel("Missing Second Operand in Division", '47 / = [1]');

  testModel("Missing Last Operand in Addition and Subtraction Chain",
            '8449 + [8449] 4205138 - [4213587] = [0]');

  testModel("Leading zeros should be collapsed",
            '0 [null null [0]] 0 [null null [0]] 0 [null null [0]] ' +
            '2 [null null [2]] 0 [null null [2 0]] + [20 + null] ' +
            '0 [20 + [0]] 0 [20 + [0]] 0 [20 + [0]] ' +
            '2 [20 + [2]] 0 [20 + [2 0]] = [40 null null]');

  testModel("Test utilities should correctly predict zeros collapse",
            '000 [[0==]] 20 [[20]] + 000 [[0==]] 20 [[20]] = [40]' +
            '00020 + 00020 = [40]');

};
