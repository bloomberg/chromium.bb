// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_id_list.h"

#include <algorithm>
#include <iterator>

#include "base/stl_util.h"

namespace device {

namespace {

static base::LazyInstance<GamepadIdList>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

constexpr struct GamepadInfo {
  uint16_t vendor;
  uint16_t product;
  XInputType xtype;
  GamepadId id;
} kGamepadInfo[] = {
    {0x0010, 0x0082, kXInputTypeNone, GamepadId::kVendor0010Product0082},
    // DragonRise Inc.
    {0x0079, 0x0006, kXInputTypeNone, GamepadId::kDragonRiseProduct0006},
    {0x0079, 0x0011, kXInputTypeNone, GamepadId::kDragonRiseProduct0011},
    {0x0079, 0x1800, kXInputTypeNone, GamepadId::kDragonRiseProduct1800},
    {0x0079, 0x181b, kXInputTypeNone, GamepadId::kDragonRiseProduct181b},
    {0x0079, 0x1843, kXInputTypeNone, GamepadId::kDragonRiseProduct1843},
    {0x0079, 0x1844, kXInputTypeNone, GamepadId::kDragonRiseProduct1844},
    // Steelseries ApS (Bluetooth)
    {0x0111, 0x1417, kXInputTypeNone, GamepadId::kSteelSeriesBtProduct1417},
    {0x0111, 0x1420, kXInputTypeNone, GamepadId::kSteelSeriesBtProduct1420},
    {0x0113, 0xf900, kXInputTypeNone, GamepadId::kVendor0113Productf900},
    // Creative Technology, Ltd
    {0x041e, 0x1003, kXInputTypeNone,
     GamepadId::kCreativeTechnologyProduct1003},
    {0x041e, 0x1050, kXInputTypeNone,
     GamepadId::kCreativeTechnologyProduct1050},
    // Advanced Gravis Computer Tech, Ltd
    {0x0428, 0x4001, kXInputTypeNone, GamepadId::kGravisProduct4001},
    // Alps Electric Co., Ltd
    {0x0433, 0x1101, kXInputTypeNone, GamepadId::kAlpsElectricProduct1101},
    // ThrustMaster, Inc.
    {0x044f, 0x0f00, kXInputTypeXbox, GamepadId::kThrustMasterProduct0f00},
    {0x044f, 0x0f03, kXInputTypeXbox, GamepadId::kThrustMasterProduct0f03},
    {0x044f, 0x0f07, kXInputTypeXbox, GamepadId::kThrustMasterProduct0f07},
    {0x044f, 0x0f10, kXInputTypeXbox, GamepadId::kThrustMasterProduct0f10},
    {0x044f, 0xa0a3, kXInputTypeNone, GamepadId::kThrustMasterProducta0a3},
    {0x044f, 0xb300, kXInputTypeNone, GamepadId::kThrustMasterProductb300},
    {0x044f, 0xb304, kXInputTypeNone, GamepadId::kThrustMasterProductb304},
    {0x044f, 0xb312, kXInputTypeNone, GamepadId::kThrustMasterProductb312},
    {0x044f, 0xb315, kXInputTypeNone, GamepadId::kThrustMasterProductb315},
    {0x044f, 0xb320, kXInputTypeNone, GamepadId::kThrustMasterProductb320},
    {0x044f, 0xb323, kXInputTypeNone, GamepadId::kThrustMasterProductb323},
    {0x044f, 0xb326, kXInputTypeXbox, GamepadId::kThrustMasterProductb326},
    {0x044f, 0xb653, kXInputTypeNone, GamepadId::kThrustMasterProductb653},
    {0x044f, 0xb677, kXInputTypeNone, GamepadId::kThrustMasterProductb677},
    {0x044f, 0xd003, kXInputTypeNone, GamepadId::kThrustMasterProductd003},
    {0x044f, 0xd008, kXInputTypeNone, GamepadId::kThrustMasterProductd008},
    {0x044f, 0xd009, kXInputTypeNone, GamepadId::kThrustMasterProductd009},
    // Microsoft Corp.
    {0x045e, 0x0026, kXInputTypeNone, GamepadId::kMicrosoftProduct0026},
    {0x045e, 0x0027, kXInputTypeNone, GamepadId::kMicrosoftProduct0027},
    {0x045e, 0x0202, kXInputTypeXbox, GamepadId::kMicrosoftProduct0202},
    {0x045e, 0x0285, kXInputTypeXbox, GamepadId::kMicrosoftProduct0285},
    {0x045e, 0x0287, kXInputTypeXbox, GamepadId::kMicrosoftProduct0287},
    {0x045e, 0x0288, kXInputTypeXbox, GamepadId::kMicrosoftProduct0288},
    {0x045e, 0x0289, kXInputTypeXbox, GamepadId::kMicrosoftProduct0289},
    {0x045e, 0x028e, kXInputTypeXbox360, GamepadId::kMicrosoftProduct028e},
    {0x045e, 0x028f, kXInputTypeNone, GamepadId::kMicrosoftProduct028f},
    {0x045e, 0x0291, kXInputTypeXbox360, GamepadId::kMicrosoftProduct0291},
    {0x045e, 0x02a0, kXInputTypeXbox360, GamepadId::kMicrosoftProduct02a0},
    {0x045e, 0x02a1, kXInputTypeXbox360, GamepadId::kMicrosoftProduct02a1},
    {0x045e, 0x02d1, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02d1},
    {0x045e, 0x02dd, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02dd},
    {0x045e, 0x02e0, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02e0},
    {0x045e, 0x02e3, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02e3},
    {0x045e, 0x02e6, kXInputTypeXbox360, GamepadId::kMicrosoftProduct02e6},
    {0x045e, 0x02ea, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02ea},
    {0x045e, 0x02fd, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02fd},
    {0x045e, 0x02ff, kXInputTypeXboxOne, GamepadId::kMicrosoftProduct02ff},
    {0x045e, 0x0719, kXInputTypeXbox360, GamepadId::kMicrosoftProduct0719},
    {0x045e, 0x0b0a, kXInputTypeXbox360, GamepadId::kMicrosoftProduct0b0a},
    // Logitech, Inc.
    {0x046d, 0xc208, kXInputTypeNone, GamepadId::kLogitechProductc208},
    {0x046d, 0xc209, kXInputTypeNone, GamepadId::kLogitechProductc209},
    {0x046d, 0xc211, kXInputTypeNone, GamepadId::kLogitechProductc211},
    {0x046d, 0xc215, kXInputTypeNone, GamepadId::kLogitechProductc215},
    {0x046d, 0xc216, kXInputTypeNone, GamepadId::kLogitechProductc216},
    {0x046d, 0xc218, kXInputTypeNone, GamepadId::kLogitechProductc218},
    {0x046d, 0xc219, kXInputTypeNone, GamepadId::kLogitechProductc219},
    {0x046d, 0xc21a, kXInputTypeNone, GamepadId::kLogitechProductc21a},
    {0x046d, 0xc21d, kXInputTypeXbox360, GamepadId::kLogitechProductc21d},
    {0x046d, 0xc21e, kXInputTypeXbox360, GamepadId::kLogitechProductc21e},
    {0x046d, 0xc21f, kXInputTypeXbox360, GamepadId::kLogitechProductc21f},
    {0x046d, 0xc242, kXInputTypeXbox360, GamepadId::kLogitechProductc242},
    {0x046d, 0xc24f, kXInputTypeNone, GamepadId::kLogitechProductc24f},
    {0x046d, 0xc260, kXInputTypeNone, GamepadId::kLogitechProductc260},
    {0x046d, 0xc261, kXInputTypeNone, GamepadId::kLogitechProductc261},
    {0x046d, 0xc262, kXInputTypeNone, GamepadId::kLogitechProductc262},
    {0x046d, 0xc298, kXInputTypeNone, GamepadId::kLogitechProductc298},
    {0x046d, 0xc299, kXInputTypeNone, GamepadId::kLogitechProductc299},
    {0x046d, 0xc29a, kXInputTypeNone, GamepadId::kLogitechProductc29a},
    {0x046d, 0xc29b, kXInputTypeNone, GamepadId::kLogitechProductc29b},
    {0x046d, 0xca84, kXInputTypeXbox, GamepadId::kLogitechProductca84},
    {0x046d, 0xca88, kXInputTypeXbox, GamepadId::kLogitechProductca88},
    {0x046d, 0xca8a, kXInputTypeXbox, GamepadId::kLogitechProductca8a},
    {0x046d, 0xcaa3, kXInputTypeXbox360, GamepadId::kLogitechProductcaa3},
    // Kensington
    {0x047d, 0x4003, kXInputTypeNone, GamepadId::kKensingtonProduct4003},
    {0x047d, 0x4005, kXInputTypeNone, GamepadId::kKensingtonProduct4005},
    // Cypress Semiconductor Corp.
    {0x04b4, 0x010a, kXInputTypeNone,
     GamepadId::kCypressSemiconductorProduct010a},
    {0x04b4, 0xd5d5, kXInputTypeNone,
     GamepadId::kCypressSemiconductorProductd5d5},
    // Holtek Semiconductor, Inc.
    {0x04d9, 0x0002, kXInputTypeNone,
     GamepadId::kHoltekSemiconductorProduct0002},
    // Samsung Electronics Co., Ltd
    {0x04e8, 0xa000, kXInputTypeNone,
     GamepadId::kSamsungElectronicsProducta000},
    // Siam United Hi-Tech
    {0x0500, 0x9b28, kXInputTypeNone, GamepadId::kSiamUnitedProduct9b28},
    // Belkin Components
    {0x050d, 0x0802, kXInputTypeNone, GamepadId::kBelkinProduct0802},
    {0x050d, 0x0803, kXInputTypeNone, GamepadId::kBelkinProduct0803},
    {0x050d, 0x0805, kXInputTypeNone, GamepadId::kBelkinProduct0805},
    // Sony Corp.
    {0x054c, 0x0268, kXInputTypeNone, GamepadId::kSonyProduct0268},
    {0x054c, 0x0306, kXInputTypeNone, GamepadId::kSonyProduct0306},
    {0x054c, 0x042f, kXInputTypeNone, GamepadId::kSonyProduct042f},
    {0x054c, 0x05c4, kXInputTypeNone, GamepadId::kSonyProduct05c4},
    {0x054c, 0x05c5, kXInputTypeNone, GamepadId::kSonyProduct05c5},
    {0x054c, 0x09cc, kXInputTypeNone, GamepadId::kSonyProduct09cc},
    {0x054c, 0x0ba0, kXInputTypeNone, GamepadId::kSonyProduct0ba0},
    // Elecom Co., Ltd
    {0x056e, 0x2003, kXInputTypeNone, GamepadId::kElecomProduct2003},
    {0x056e, 0x2004, kXInputTypeXbox360, GamepadId::kElecomProduct2004},
    // Nintendo Co., Ltd
    {0x057e, 0x0306, kXInputTypeNone, GamepadId::kNintendoProduct0306},
    {0x057e, 0x0330, kXInputTypeNone, GamepadId::kNintendoProduct0330},
    {0x057e, 0x0337, kXInputTypeNone, GamepadId::kNintendoProduct0337},
    {0x057e, 0x2006, kXInputTypeNone, GamepadId::kNintendoProduct2006},
    {0x057e, 0x2007, kXInputTypeNone, GamepadId::kNintendoProduct2007},
    {0x057e, 0x2009, kXInputTypeNone, GamepadId::kNintendoProduct2009},
    {0x057e, 0x200e, kXInputTypeNone, GamepadId::kNintendoProduct200e},
    // Padix Co., Ltd (Rockfire)
    {0x0583, 0x2060, kXInputTypeNone, GamepadId::kPadixProduct2060},
    {0x0583, 0x206f, kXInputTypeNone, GamepadId::kPadixProduct206f},
    {0x0583, 0x3050, kXInputTypeNone, GamepadId::kPadixProduct3050},
    {0x0583, 0xa000, kXInputTypeNone, GamepadId::kPadixProducta000},
    {0x0583, 0xa024, kXInputTypeNone, GamepadId::kPadixProducta024},
    {0x0583, 0xa025, kXInputTypeNone, GamepadId::kPadixProducta025},
    {0x0583, 0xa130, kXInputTypeNone, GamepadId::kPadixProducta130},
    {0x0583, 0xa133, kXInputTypeNone, GamepadId::kPadixProducta133},
    {0x0583, 0xb031, kXInputTypeNone, GamepadId::kPadixProductb031},
    // Vetronix Corp.
    {0x05a0, 0x3232, kXInputTypeNone, GamepadId::kVetronixProduct3232},
    // Apple, Inc.
    {0x05ac, 0x3232, kXInputTypeNone, GamepadId::kAppleProduct3232},
    // Genesys Logic, Inc.
    {0x05e3, 0x0596, kXInputTypeNone, GamepadId::kGenesysLogicProduct0596},
    // InterAct, Inc.
    {0x05fd, 0x1007, kXInputTypeXbox, GamepadId::kInterActProduct1007},
    {0x05fd, 0x107a, kXInputTypeXbox, GamepadId::kInterActProduct107a},
    {0x05fd, 0x3000, kXInputTypeNone, GamepadId::kInterActProduct3000},
    // Chic Technology Corp.
    {0x05fe, 0x0014, kXInputTypeNone, GamepadId::kChicTechnologyProduct0014},
    {0x05fe, 0x3030, kXInputTypeXbox, GamepadId::kChicTechnologyProduct3030},
    {0x05fe, 0x3031, kXInputTypeXbox, GamepadId::kChicTechnologyProduct3031},
    // MosArt Semiconductor Corp.
    {0x062a, 0x0020, kXInputTypeXbox,
     GamepadId::kMosArtSemiconductorProduct0020},
    {0x062a, 0x0033, kXInputTypeXbox,
     GamepadId::kMosArtSemiconductorProduct0033},
    {0x062a, 0x2410, kXInputTypeNone,
     GamepadId::kMosArtSemiconductorProduct2410},
    // Saitek PLC
    {0x06a3, 0x0109, kXInputTypeNone, GamepadId::kSaitekProduct0109},
    {0x06a3, 0x0200, kXInputTypeXbox, GamepadId::kSaitekProduct0200},
    {0x06a3, 0x0201, kXInputTypeXbox, GamepadId::kSaitekProduct0201},
    {0x06a3, 0x0241, kXInputTypeNone, GamepadId::kSaitekProduct0241},
    {0x06a3, 0x040b, kXInputTypeNone, GamepadId::kSaitekProduct040b},
    {0x06a3, 0x040c, kXInputTypeNone, GamepadId::kSaitekProduct040c},
    {0x06a3, 0x052d, kXInputTypeNone, GamepadId::kSaitekProduct052d},
    {0x06a3, 0x3509, kXInputTypeNone, GamepadId::kSaitekProduct3509},
    {0x06a3, 0xf518, kXInputTypeNone, GamepadId::kSaitekProductf518},
    {0x06a3, 0xf51a, kXInputTypeXbox360, GamepadId::kSaitekProductf51a},
    {0x06a3, 0xf622, kXInputTypeNone, GamepadId::kSaitekProductf622},
    {0x06a3, 0xf623, kXInputTypeNone, GamepadId::kSaitekProductf623},
    {0x06a3, 0xff0c, kXInputTypeNone, GamepadId::kSaitekProductff0c},
    // Aashima Technology B.V.
    {0x06d6, 0x0025, kXInputTypeNone, GamepadId::kTrustProduct0025},
    {0x06d6, 0x0026, kXInputTypeNone, GamepadId::kTrustProduct0026},
    // Guillemot Corp.
    {0x06f8, 0xa300, kXInputTypeNone, GamepadId::kGuillemotProducta300},
    // Mad Catz, Inc.
    {0x0738, 0x3250, kXInputTypeNone, GamepadId::kMadCatzProduct3250},
    {0x0738, 0x3285, kXInputTypeNone, GamepadId::kMadCatzProduct3285},
    {0x0738, 0x3384, kXInputTypeNone, GamepadId::kMadCatzProduct3384},
    {0x0738, 0x3480, kXInputTypeNone, GamepadId::kMadCatzProduct3480},
    {0x0738, 0x3481, kXInputTypeNone, GamepadId::kMadCatzProduct3481},
    {0x0738, 0x4506, kXInputTypeXbox, GamepadId::kMadCatzProduct4506},
    {0x0738, 0x4516, kXInputTypeXbox, GamepadId::kMadCatzProduct4516},
    {0x0738, 0x4520, kXInputTypeXbox, GamepadId::kMadCatzProduct4520},
    {0x0738, 0x4522, kXInputTypeXbox, GamepadId::kMadCatzProduct4522},
    {0x0738, 0x4526, kXInputTypeXbox, GamepadId::kMadCatzProduct4526},
    {0x0738, 0x4530, kXInputTypeXbox, GamepadId::kMadCatzProduct4530},
    {0x0738, 0x4536, kXInputTypeXbox, GamepadId::kMadCatzProduct4536},
    {0x0738, 0x4540, kXInputTypeXbox, GamepadId::kMadCatzProduct4540},
    {0x0738, 0x4556, kXInputTypeXbox, GamepadId::kMadCatzProduct4556},
    {0x0738, 0x4586, kXInputTypeXbox, GamepadId::kMadCatzProduct4586},
    {0x0738, 0x4588, kXInputTypeXbox, GamepadId::kMadCatzProduct4588},
    {0x0738, 0x45ff, kXInputTypeXbox, GamepadId::kMadCatzProduct45ff},
    {0x0738, 0x4716, kXInputTypeXbox360, GamepadId::kMadCatzProduct4716},
    {0x0738, 0x4718, kXInputTypeXbox360, GamepadId::kMadCatzProduct4718},
    {0x0738, 0x4726, kXInputTypeXbox360, GamepadId::kMadCatzProduct4726},
    {0x0738, 0x4728, kXInputTypeXbox360, GamepadId::kMadCatzProduct4728},
    {0x0738, 0x4736, kXInputTypeXbox360, GamepadId::kMadCatzProduct4736},
    {0x0738, 0x4738, kXInputTypeXbox360, GamepadId::kMadCatzProduct4738},
    {0x0738, 0x4740, kXInputTypeXbox360, GamepadId::kMadCatzProduct4740},
    {0x0738, 0x4743, kXInputTypeXbox, GamepadId::kMadCatzProduct4743},
    {0x0738, 0x4758, kXInputTypeXbox360, GamepadId::kMadCatzProduct4758},
    {0x0738, 0x4a01, kXInputTypeXboxOne, GamepadId::kMadCatzProduct4a01},
    {0x0738, 0x5266, kXInputTypeNone, GamepadId::kMadCatzProduct5266},
    {0x0738, 0x6040, kXInputTypeXbox, GamepadId::kMadCatzProduct6040},
    {0x0738, 0x8180, kXInputTypeNone, GamepadId::kMadCatzProduct8180},
    {0x0738, 0x8250, kXInputTypeNone, GamepadId::kMadCatzProduct8250},
    {0x0738, 0x8384, kXInputTypeNone, GamepadId::kMadCatzProduct8384},
    {0x0738, 0x8480, kXInputTypeNone, GamepadId::kMadCatzProduct8480},
    {0x0738, 0x8481, kXInputTypeNone, GamepadId::kMadCatzProduct8481},
    {0x0738, 0x8818, kXInputTypeNone, GamepadId::kMadCatzProduct8818},
    {0x0738, 0x8838, kXInputTypeNone, GamepadId::kMadCatzProduct8838},
    {0x0738, 0x9871, kXInputTypeXbox360, GamepadId::kMadCatzProduct9871},
    {0x0738, 0xb726, kXInputTypeXbox360, GamepadId::kMadCatzProductb726},
    {0x0738, 0xb738, kXInputTypeXbox360, GamepadId::kMadCatzProductb738},
    {0x0738, 0xbeef, kXInputTypeXbox360, GamepadId::kMadCatzProductbeef},
    {0x0738, 0xcb02, kXInputTypeXbox360, GamepadId::kMadCatzProductcb02},
    {0x0738, 0xcb03, kXInputTypeXbox360, GamepadId::kMadCatzProductcb03},
    {0x0738, 0xcb29, kXInputTypeXbox360, GamepadId::kMadCatzProductcb29},
    {0x0738, 0xf401, kXInputTypeNone, GamepadId::kMadCatzProductf401},
    {0x0738, 0xf738, kXInputTypeXbox360, GamepadId::kMadCatzProductf738},
    {0x07b5, 0x0213, kXInputTypeNone, GamepadId::kMegaWorldProduct0213},
    {0x07b5, 0x0312, kXInputTypeNone, GamepadId::kMegaWorldProduct0312},
    {0x07b5, 0x0314, kXInputTypeNone, GamepadId::kMegaWorldProduct0314},
    {0x07b5, 0x0315, kXInputTypeNone, GamepadId::kMegaWorldProduct0315},
    {0x07b5, 0x9902, kXInputTypeNone, GamepadId::kMegaWorldProduct9902},
    {0x07ff, 0xffff, kXInputTypeXbox360, GamepadId::kVendor07ffProductffff},
    // Personal Communication Systems, Inc.
    {0x0810, 0x0001, kXInputTypeNone,
     GamepadId::kPersonalCommunicationSystemsProduct0001},
    {0x0810, 0x0003, kXInputTypeNone,
     GamepadId::kPersonalCommunicationSystemsProduct0003},
    {0x0810, 0x1e01, kXInputTypeNone,
     GamepadId::kPersonalCommunicationSystemsProduct1e01},
    {0x0810, 0xe501, kXInputTypeNone,
     GamepadId::kPersonalCommunicationSystemsProducte501},
    // Lakeview Research
    {0x0925, 0x0005, kXInputTypeNone, GamepadId::kLakeviewResearchProduct0005},
    {0x0925, 0x03e8, kXInputTypeNone, GamepadId::kLakeviewResearchProduct03e8},
    {0x0925, 0x1700, kXInputTypeNone, GamepadId::kLakeviewResearchProduct1700},
    {0x0925, 0x2801, kXInputTypeNone, GamepadId::kLakeviewResearchProduct2801},
    {0x0925, 0x8866, kXInputTypeNone, GamepadId::kLakeviewResearchProduct8866},
    {0x0926, 0x2526, kXInputTypeNone, GamepadId::kVendor0926Product2526},
    {0x0926, 0x8888, kXInputTypeNone, GamepadId::kVendor0926Product8888},
    // NVIDIA Corp.
    {0x0955, 0x7210, kXInputTypeNone, GamepadId::kNvidiaProduct7210},
    {0x0955, 0x7214, kXInputTypeNone, GamepadId::kNvidiaProduct7214},
    // ASUSTek Computer, Inc.
    {0x0b05, 0x4500, kXInputTypeNone, GamepadId::kAsusTekProduct4500},
    // Play.com, Inc.
    {0x0b43, 0x0005, kXInputTypeNone, GamepadId::kPlayComProduct0005},
    // Zeroplus
    {0x0c12, 0x0005, kXInputTypeXbox, GamepadId::kZeroplusProduct0005},
    {0x0c12, 0x0e10, kXInputTypeNone, GamepadId::kZeroplusProduct0e10},
    {0x0c12, 0x0ef6, kXInputTypeNone, GamepadId::kZeroplusProduct0ef6},
    {0x0c12, 0x1cf6, kXInputTypeNone, GamepadId::kZeroplusProduct1cf6},
    {0x0c12, 0x8801, kXInputTypeXbox, GamepadId::kZeroplusProduct8801},
    {0x0c12, 0x8802, kXInputTypeXbox, GamepadId::kZeroplusProduct8802},
    {0x0c12, 0x8809, kXInputTypeXbox, GamepadId::kZeroplusProduct8809},
    {0x0c12, 0x880a, kXInputTypeXbox, GamepadId::kZeroplusProduct880a},
    {0x0c12, 0x8810, kXInputTypeXbox, GamepadId::kZeroplusProduct8810},
    {0x0c12, 0x9902, kXInputTypeXbox, GamepadId::kZeroplusProduct9902},
    // Microdia
    {0x0c45, 0x4320, kXInputTypeNone, GamepadId::kMicrodiaProduct4320},
    {0x0d2f, 0x0002, kXInputTypeXbox, GamepadId::kVendor0d2fProduct0002},
    // Radica Games, Ltd
    {0x0e4c, 0x1097, kXInputTypeXbox, GamepadId::kRadicaGamesProduct1097},
    {0x0e4c, 0x1103, kXInputTypeXbox, GamepadId::kRadicaGamesProduct1103},
    {0x0e4c, 0x2390, kXInputTypeXbox, GamepadId::kRadicaGamesProduct2390},
    {0x0e4c, 0x3510, kXInputTypeXbox, GamepadId::kRadicaGamesProduct3510},
    // Logic3
    {0x0e6f, 0x0003, kXInputTypeXbox, GamepadId::kPdpProduct0003},
    {0x0e6f, 0x0005, kXInputTypeXbox, GamepadId::kPdpProduct0005},
    {0x0e6f, 0x0006, kXInputTypeXbox, GamepadId::kPdpProduct0006},
    {0x0e6f, 0x0008, kXInputTypeXbox, GamepadId::kPdpProduct0008},
    {0x0e6f, 0x0105, kXInputTypeXbox360, GamepadId::kPdpProduct0105},
    {0x0e6f, 0x0113, kXInputTypeXbox360, GamepadId::kPdpProduct0113},
    {0x0e6f, 0x011e, kXInputTypeNone, GamepadId::kPdpProduct011e},
    {0x0e6f, 0x011f, kXInputTypeXbox360, GamepadId::kPdpProduct011f},
    {0x0e6f, 0x0124, kXInputTypeNone, GamepadId::kPdpProduct0124},
    {0x0e6f, 0x0130, kXInputTypeNone, GamepadId::kPdpProduct0130},
    {0x0e6f, 0x0131, kXInputTypeXbox360, GamepadId::kPdpProduct0131},
    {0x0e6f, 0x0133, kXInputTypeXbox360, GamepadId::kPdpProduct0133},
    {0x0e6f, 0x0139, kXInputTypeXboxOne, GamepadId::kPdpProduct0139},
    {0x0e6f, 0x013a, kXInputTypeXboxOne, GamepadId::kPdpProduct013a},
    {0x0e6f, 0x0146, kXInputTypeXboxOne, GamepadId::kPdpProduct0146},
    {0x0e6f, 0x0147, kXInputTypeXboxOne, GamepadId::kPdpProduct0147},
    {0x0e6f, 0x0158, kXInputTypeNone, GamepadId::kPdpProduct0158},
    {0x0e6f, 0x015c, kXInputTypeXboxOne, GamepadId::kPdpProduct015c},
    {0x0e6f, 0x0161, kXInputTypeXboxOne, GamepadId::kPdpProduct0161},
    {0x0e6f, 0x0162, kXInputTypeXboxOne, GamepadId::kPdpProduct0162},
    {0x0e6f, 0x0163, kXInputTypeXboxOne, GamepadId::kPdpProduct0163},
    {0x0e6f, 0x0164, kXInputTypeXboxOne, GamepadId::kPdpProduct0164},
    {0x0e6f, 0x0165, kXInputTypeXboxOne, GamepadId::kPdpProduct0165},
    {0x0e6f, 0x0201, kXInputTypeXbox360, GamepadId::kPdpProduct0201},
    {0x0e6f, 0x0213, kXInputTypeXbox360, GamepadId::kPdpProduct0213},
    {0x0e6f, 0x021f, kXInputTypeXbox360, GamepadId::kPdpProduct021f},
    {0x0e6f, 0x0246, kXInputTypeXboxOne, GamepadId::kPdpProduct0246},
    {0x0e6f, 0x02a0, kXInputTypeNone, GamepadId::kPdpProduct02a0},
    {0x0e6f, 0x02ab, kXInputTypeNone, GamepadId::kPdpProduct02ab},
    {0x0e6f, 0x0301, kXInputTypeXbox360, GamepadId::kPdpProduct0301},
    {0x0e6f, 0x0346, kXInputTypeXboxOne, GamepadId::kPdpProduct0346},
    {0x0e6f, 0x0401, kXInputTypeXbox360, GamepadId::kPdpProduct0401},
    {0x0e6f, 0x0413, kXInputTypeXbox360, GamepadId::kPdpProduct0413},
    {0x0e6f, 0x0501, kXInputTypeXbox360, GamepadId::kPdpProduct0501},
    {0x0e6f, 0xf501, kXInputTypeNone, GamepadId::kPdpProductf501},
    {0x0e6f, 0xf701, kXInputTypeNone, GamepadId::kPdpProductf701},
    {0x0e6f, 0xf900, kXInputTypeXbox360, GamepadId::kPdpProductf900},
    // GreenAsia Inc.
    {0x0e8f, 0x0003, kXInputTypeNone, GamepadId::kGreenAsiaProduct0003},
    {0x0e8f, 0x0008, kXInputTypeNone, GamepadId::kGreenAsiaProduct0008},
    {0x0e8f, 0x0012, kXInputTypeNone, GamepadId::kGreenAsiaProduct0012},
    {0x0e8f, 0x0201, kXInputTypeXbox, GamepadId::kGreenAsiaProduct0201},
    {0x0e8f, 0x1006, kXInputTypeNone, GamepadId::kGreenAsiaProduct1006},
    {0x0e8f, 0x3008, kXInputTypeXbox, GamepadId::kGreenAsiaProduct3008},
    {0x0e8f, 0x3010, kXInputTypeNone, GamepadId::kGreenAsiaProduct3010},
    {0x0e8f, 0x3013, kXInputTypeNone, GamepadId::kGreenAsiaProduct3013},
    {0x0e8f, 0x3075, kXInputTypeNone, GamepadId::kGreenAsiaProduct3075},
    {0x0e8f, 0x310d, kXInputTypeNone, GamepadId::kGreenAsiaProduct310d},
    // Hori Co., Ltd
    {0x0f0d, 0x000a, kXInputTypeXbox360, GamepadId::kHoriProduct000a},
    {0x0f0d, 0x000c, kXInputTypeXbox360, GamepadId::kHoriProduct000c},
    {0x0f0d, 0x000d, kXInputTypeXbox360, GamepadId::kHoriProduct000d},
    {0x0f0d, 0x0010, kXInputTypeNone, GamepadId::kHoriProduct0010},
    {0x0f0d, 0x0011, kXInputTypeNone, GamepadId::kHoriProduct0011},
    {0x0f0d, 0x0016, kXInputTypeXbox360, GamepadId::kHoriProduct0016},
    {0x0f0d, 0x001b, kXInputTypeXbox360, GamepadId::kHoriProduct001b},
    {0x0f0d, 0x0022, kXInputTypeNone, GamepadId::kHoriProduct0022},
    {0x0f0d, 0x0027, kXInputTypeNone, GamepadId::kHoriProduct0027},
    {0x0f0d, 0x003d, kXInputTypeNone, GamepadId::kHoriProduct003d},
    {0x0f0d, 0x0040, kXInputTypeNone, GamepadId::kHoriProduct0040},
    {0x0f0d, 0x0049, kXInputTypeNone, GamepadId::kHoriProduct0049},
    {0x0f0d, 0x004d, kXInputTypeNone, GamepadId::kHoriProduct004d},
    {0x0f0d, 0x0055, kXInputTypeNone, GamepadId::kHoriProduct0055},
    {0x0f0d, 0x005b, kXInputTypeNone, GamepadId::kHoriProduct005b},
    {0x0f0d, 0x005c, kXInputTypeNone, GamepadId::kHoriProduct005c},
    {0x0f0d, 0x005e, kXInputTypeNone, GamepadId::kHoriProduct005e},
    {0x0f0d, 0x005f, kXInputTypeNone, GamepadId::kHoriProduct005f},
    {0x0f0d, 0x0063, kXInputTypeXboxOne, GamepadId::kHoriProduct0063},
    {0x0f0d, 0x0066, kXInputTypeNone, GamepadId::kHoriProduct0066},
    {0x0f0d, 0x0067, kXInputTypeXboxOne, GamepadId::kHoriProduct0067},
    {0x0f0d, 0x006a, kXInputTypeNone, GamepadId::kHoriProduct006a},
    {0x0f0d, 0x006b, kXInputTypeNone, GamepadId::kHoriProduct006b},
    {0x0f0d, 0x006e, kXInputTypeNone, GamepadId::kHoriProduct006e},
    {0x0f0d, 0x0070, kXInputTypeNone, GamepadId::kHoriProduct0070},
    {0x0f0d, 0x0078, kXInputTypeXboxOne, GamepadId::kHoriProduct0078},
    {0x0f0d, 0x0084, kXInputTypeNone, GamepadId::kHoriProduct0084},
    {0x0f0d, 0x0085, kXInputTypeNone, GamepadId::kHoriProduct0085},
    {0x0f0d, 0x0087, kXInputTypeNone, GamepadId::kHoriProduct0087},
    {0x0f0d, 0x0088, kXInputTypeNone, GamepadId::kHoriProduct0088},
    {0x0f0d, 0x008a, kXInputTypeNone, GamepadId::kHoriProduct008a},
    {0x0f0d, 0x008b, kXInputTypeNone, GamepadId::kHoriProduct008b},
    {0x0f0d, 0x0090, kXInputTypeNone, GamepadId::kHoriProduct0090},
    {0x0f0d, 0x00ee, kXInputTypeNone, GamepadId::kHoriProduct00ee},
    // Jess Technology Co., Ltd
    {0x0f30, 0x010b, kXInputTypeXbox, GamepadId::kJessTechnologyProduct010b},
    {0x0f30, 0x0110, kXInputTypeNone, GamepadId::kJessTechnologyProduct0110},
    {0x0f30, 0x0111, kXInputTypeNone, GamepadId::kJessTechnologyProduct0111},
    {0x0f30, 0x0112, kXInputTypeNone, GamepadId::kJessTechnologyProduct0112},
    {0x0f30, 0x0202, kXInputTypeXbox, GamepadId::kJessTechnologyProduct0202},
    {0x0f30, 0x0208, kXInputTypeNone, GamepadId::kJessTechnologyProduct0208},
    {0x0f30, 0x1012, kXInputTypeNone, GamepadId::kJessTechnologyProduct1012},
    {0x0f30, 0x1100, kXInputTypeNone, GamepadId::kJessTechnologyProduct1100},
    {0x0f30, 0x1112, kXInputTypeNone, GamepadId::kJessTechnologyProduct1112},
    {0x0f30, 0x1116, kXInputTypeNone, GamepadId::kJessTechnologyProduct1116},
    {0x0f30, 0x8888, kXInputTypeXbox, GamepadId::kJessTechnologyProduct8888},
    // Etoms Electronics Corp.
    {0x102c, 0xff0c, kXInputTypeXbox, GamepadId::kEtomsElectronicsProductff0c},
    // SteelSeries ApS
    {0x1038, 0x1412, kXInputTypeNone, GamepadId::kSteelSeriesProduct1412},
    {0x1038, 0x1418, kXInputTypeNone, GamepadId::kSteelSeriesProduct1418},
    {0x1038, 0x1420, kXInputTypeNone, GamepadId::kSteelSeriesProduct1420},
    {0x1080, 0x0009, kXInputTypeNone, GamepadId::kVendor1080Product0009},
    // Betop
    {0x11c0, 0x5213, kXInputTypeNone, GamepadId::kBetopProduct5213},
    {0x11c0, 0x5506, kXInputTypeNone, GamepadId::kBetopProduct5506},
    {0x11c9, 0x55f0, kXInputTypeXbox360, GamepadId::kVendor11c9Product55f0},
    {0x11ff, 0x3331, kXInputTypeNone, GamepadId::kVendor11ffProduct3331},
    {0x11ff, 0x3341, kXInputTypeNone, GamepadId::kVendor11ffProduct3341},
    // Focusrite-Novation
    {0x1235, 0xab21, kXInputTypeNone, GamepadId::kFocusriteNovationProductab21},
    // Nyko (Honey Bee)
    {0x124b, 0x4d01, kXInputTypeNone, GamepadId::kNykoProduct4d01},
    // Honey Bee Electronic International Ltd.
    {0x12ab, 0x0004, kXInputTypeXbox360, GamepadId::kHoneyBeeProduct0004},
    {0x12ab, 0x0006, kXInputTypeNone, GamepadId::kHoneyBeeProduct0006},
    {0x12ab, 0x0301, kXInputTypeXbox360, GamepadId::kHoneyBeeProduct0301},
    {0x12ab, 0x0302, kXInputTypeNone, GamepadId::kHoneyBeeProduct0302},
    {0x12ab, 0x0303, kXInputTypeXbox360, GamepadId::kHoneyBeeProduct0303},
    {0x12ab, 0x0e6f, kXInputTypeNone, GamepadId::kHoneyBeeProduct0e6f},
    {0x12ab, 0x8809, kXInputTypeXbox, GamepadId::kHoneyBeeProduct8809},
    // Gembird
    {0x12bd, 0xd012, kXInputTypeNone, GamepadId::kGembirdProductd012},
    {0x12bd, 0xd015, kXInputTypeNone, GamepadId::kGembirdProductd015},
    // Sino Lite Technology Corp.
    {0x1345, 0x1000, kXInputTypeNone, GamepadId::kSinoLiteProduct1000},
    {0x1345, 0x3008, kXInputTypeNone, GamepadId::kSinoLiteProduct3008},
    // RedOctane
    {0x1430, 0x02a0, kXInputTypeNone, GamepadId::kRedOctaneProduct02a0},
    {0x1430, 0x4734, kXInputTypeNone, GamepadId::kRedOctaneProduct4734},
    {0x1430, 0x4748, kXInputTypeXbox360, GamepadId::kRedOctaneProduct4748},
    {0x1430, 0x474c, kXInputTypeNone, GamepadId::kRedOctaneProduct474c},
    {0x1430, 0x8888, kXInputTypeXbox, GamepadId::kRedOctaneProduct8888},
    {0x1430, 0xf801, kXInputTypeXbox360, GamepadId::kRedOctaneProductf801},
    {0x146b, 0x0601, kXInputTypeXbox360, GamepadId::kVendor146bProduct0601},
    {0x146b, 0x0d01, kXInputTypeNone, GamepadId::kVendor146bProduct0d01},
    {0x146b, 0x5500, kXInputTypeNone, GamepadId::kVendor146bProduct5500},
    // JAMER INDUSTRIES CO., LTD.
    {0x14d8, 0x6208, kXInputTypeNone, GamepadId::kJamerIndustriesProduct6208},
    {0x14d8, 0xcd07, kXInputTypeNone, GamepadId::kJamerIndustriesProductcd07},
    {0x14d8, 0xcfce, kXInputTypeNone, GamepadId::kJamerIndustriesProductcfce},
    // Razer USA, Ltd
    {0x1532, 0x0037, kXInputTypeXbox360, GamepadId::kRazer1532Product0037},
    {0x1532, 0x0300, kXInputTypeNone, GamepadId::kRazer1532Product0300},
    {0x1532, 0x0401, kXInputTypeNone, GamepadId::kRazer1532Product0401},
    {0x1532, 0x0900, kXInputTypeNone, GamepadId::kRazer1532Product0900},
    {0x1532, 0x0a00, kXInputTypeXboxOne, GamepadId::kRazer1532Product0a00},
    {0x1532, 0x0a03, kXInputTypeXboxOne, GamepadId::kRazer1532Product0a03},
    {0x1532, 0x1000, kXInputTypeNone, GamepadId::kRazer1532Product1000},
    {0x15e4, 0x3f00, kXInputTypeXbox360, GamepadId::kNumarkProduct3f00},
    {0x15e4, 0x3f0a, kXInputTypeXbox360, GamepadId::kNumarkProduct3f0a},
    {0x15e4, 0x3f10, kXInputTypeXbox360, GamepadId::kNumarkProduct3f10},
    {0x162e, 0xbeef, kXInputTypeXbox360, GamepadId::kVendor162eProductbeef},
    // Razer USA, Ltd
    {0x1689, 0x0001, kXInputTypeNone, GamepadId::kRazer1689Product0001},
    {0x1689, 0xfd00, kXInputTypeXbox360, GamepadId::kRazer1689Productfd00},
    {0x1689, 0xfd01, kXInputTypeXbox360, GamepadId::kRazer1689Productfd01},
    {0x1689, 0xfe00, kXInputTypeXbox360, GamepadId::kRazer1689Productfe00},
    // Askey Computer Corp.
    {0x1690, 0x0001, kXInputTypeNone, GamepadId::kAskeyComputerProduct0001},
    // Van Ooijen Technische Informatica
    {0x16c0, 0x0487, kXInputTypeNone, GamepadId::kVanOoijenProduct0487},
    {0x16c0, 0x05e1, kXInputTypeNone, GamepadId::kVanOoijenProduct05e1},
    {0x1781, 0x057e, kXInputTypeNone, GamepadId::kVendor1781Product057e},
    // Google Inc.
    {0x18d1, 0x2c40, kXInputTypeNone, GamepadId::kGoogleProduct2c40},
    // Lab126, Inc.
    {0x1949, 0x0402, kXInputTypeNone, GamepadId::kLab126Product0402},
    // Gampaq Co.Ltd
    {0x19fa, 0x0607, kXInputTypeNone, GamepadId::kGampaqProduct0607},
    // ACRUX
    {0x1a34, 0x0203, kXInputTypeNone, GamepadId::kAcruxProduct0203},
    {0x1a34, 0x0401, kXInputTypeNone, GamepadId::kAcruxProduct0401},
    {0x1a34, 0x0801, kXInputTypeNone, GamepadId::kAcruxProduct0801},
    {0x1a34, 0x0802, kXInputTypeNone, GamepadId::kAcruxProduct0802},
    {0x1a34, 0x0836, kXInputTypeNone, GamepadId::kAcruxProduct0836},
    {0x1a34, 0xf705, kXInputTypeNone, GamepadId::kAcruxProductf705},
    // Harmonix Music
    {0x1bad, 0x0002, kXInputTypeXbox360, GamepadId::kHarmonixMusicProduct0002},
    {0x1bad, 0x0003, kXInputTypeXbox360, GamepadId::kHarmonixMusicProduct0003},
    {0x1bad, 0x0130, kXInputTypeXbox360, GamepadId::kHarmonixMusicProduct0130},
    {0x1bad, 0x028e, kXInputTypeNone, GamepadId::kHarmonixMusicProduct028e},
    {0x1bad, 0x0301, kXInputTypeNone, GamepadId::kHarmonixMusicProduct0301},
    {0x1bad, 0x5500, kXInputTypeNone, GamepadId::kHarmonixMusicProduct5500},
    {0x1bad, 0xf016, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf016},
    {0x1bad, 0xf018, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf018},
    {0x1bad, 0xf019, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf019},
    {0x1bad, 0xf021, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf021},
    {0x1bad, 0xf023, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf023},
    {0x1bad, 0xf025, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf025},
    {0x1bad, 0xf027, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf027},
    {0x1bad, 0xf028, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf028},
    {0x1bad, 0xf02d, kXInputTypeNone, GamepadId::kHarmonixMusicProductf02d},
    {0x1bad, 0xf02e, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf02e},
    {0x1bad, 0xf030, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf030},
    {0x1bad, 0xf036, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf036},
    {0x1bad, 0xf038, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf038},
    {0x1bad, 0xf039, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf039},
    {0x1bad, 0xf03a, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf03a},
    {0x1bad, 0xf03d, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf03d},
    {0x1bad, 0xf03e, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf03e},
    {0x1bad, 0xf03f, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf03f},
    {0x1bad, 0xf042, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf042},
    {0x1bad, 0xf080, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf080},
    {0x1bad, 0xf0ca, kXInputTypeNone, GamepadId::kHarmonixMusicProductf0ca},
    {0x1bad, 0xf501, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf501},
    {0x1bad, 0xf502, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf502},
    {0x1bad, 0xf503, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf503},
    {0x1bad, 0xf504, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf504},
    {0x1bad, 0xf505, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf505},
    {0x1bad, 0xf506, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf506},
    {0x1bad, 0xf900, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf900},
    {0x1bad, 0xf901, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf901},
    {0x1bad, 0xf902, kXInputTypeNone, GamepadId::kHarmonixMusicProductf902},
    {0x1bad, 0xf903, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf903},
    {0x1bad, 0xf904, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf904},
    {0x1bad, 0xf906, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductf906},
    {0x1bad, 0xf907, kXInputTypeNone, GamepadId::kHarmonixMusicProductf907},
    {0x1bad, 0xfa01, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductfa01},
    {0x1bad, 0xfd00, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductfd00},
    {0x1bad, 0xfd01, kXInputTypeXbox360, GamepadId::kHarmonixMusicProductfd01},
    // OpenMoko, Inc.
    {0x1d50, 0x6053, kXInputTypeNone, GamepadId::kOpenMokoProduct6053},
    {0x1d79, 0x0301, kXInputTypeNone, GamepadId::kVendor1d79Product0301},
    {0x1dd8, 0x000b, kXInputTypeNone, GamepadId::kVendor1dd8Product000b},
    {0x1dd8, 0x000f, kXInputTypeNone, GamepadId::kVendor1dd8Product000f},
    {0x1dd8, 0x0010, kXInputTypeNone, GamepadId::kVendor1dd8Product0010},
    // DAP Technologies
    {0x2002, 0x9000, kXInputTypeNone, GamepadId::kDapTechnologiesProduct9000},
    {0x20d6, 0x0dad, kXInputTypeNone, GamepadId::kVendor20d6Product0dad},
    {0x20d6, 0x6271, kXInputTypeNone, GamepadId::kVendor20d6Product6271},
    {0x20d6, 0x89e5, kXInputTypeNone, GamepadId::kVendor20d6Product89e5},
    {0x20d6, 0xca6d, kXInputTypeNone, GamepadId::kVendor20d6Productca6d},
    {0x20e8, 0x5860, kXInputTypeNone, GamepadId::kVendor20e8Product5860},
    // MacAlly
    {0x2222, 0x0060, kXInputTypeNone, GamepadId::kMacAllyProduct0060},
    {0x2222, 0x4010, kXInputTypeNone, GamepadId::kMacAllyProduct4010},
    {0x22ba, 0x1020, kXInputTypeNone, GamepadId::kVendor22baProduct1020},
    {0x2378, 0x1008, kXInputTypeNone, GamepadId::kVendor2378Product1008},
    {0x2378, 0x100a, kXInputTypeNone, GamepadId::kVendor2378Product100a},
    {0x24c6, 0x5000, kXInputTypeXbox360, GamepadId::kVendor24c6Product5000},
    {0x24c6, 0x5300, kXInputTypeXbox360, GamepadId::kVendor24c6Product5300},
    {0x24c6, 0x5303, kXInputTypeXbox360, GamepadId::kVendor24c6Product5303},
    {0x24c6, 0x530a, kXInputTypeXbox360, GamepadId::kVendor24c6Product530a},
    {0x24c6, 0x531a, kXInputTypeXbox360, GamepadId::kVendor24c6Product531a},
    {0x24c6, 0x5397, kXInputTypeXbox360, GamepadId::kVendor24c6Product5397},
    {0x24c6, 0x541a, kXInputTypeXboxOne, GamepadId::kVendor24c6Product541a},
    {0x24c6, 0x542a, kXInputTypeXboxOne, GamepadId::kVendor24c6Product542a},
    {0x24c6, 0x543a, kXInputTypeXboxOne, GamepadId::kVendor24c6Product543a},
    {0x24c6, 0x5500, kXInputTypeXbox360, GamepadId::kVendor24c6Product5500},
    {0x24c6, 0x5501, kXInputTypeXbox360, GamepadId::kVendor24c6Product5501},
    {0x24c6, 0x5502, kXInputTypeXbox360, GamepadId::kVendor24c6Product5502},
    {0x24c6, 0x5503, kXInputTypeXbox360, GamepadId::kVendor24c6Product5503},
    {0x24c6, 0x5506, kXInputTypeXbox360, GamepadId::kVendor24c6Product5506},
    {0x24c6, 0x550d, kXInputTypeXbox360, GamepadId::kVendor24c6Product550d},
    {0x24c6, 0x550e, kXInputTypeXbox360, GamepadId::kVendor24c6Product550e},
    {0x24c6, 0x551a, kXInputTypeXboxOne, GamepadId::kVendor24c6Product551a},
    {0x24c6, 0x561a, kXInputTypeXboxOne, GamepadId::kVendor24c6Product561a},
    {0x24c6, 0x5b00, kXInputTypeXbox360, GamepadId::kVendor24c6Product5b00},
    {0x24c6, 0x5b02, kXInputTypeXbox360, GamepadId::kVendor24c6Product5b02},
    {0x24c6, 0x5b03, kXInputTypeXbox360, GamepadId::kVendor24c6Product5b03},
    {0x24c6, 0x5d04, kXInputTypeXbox360, GamepadId::kVendor24c6Product5d04},
    {0x24c6, 0xfafb, kXInputTypeNone, GamepadId::kVendor24c6Productfafb},
    {0x24c6, 0xfafc, kXInputTypeNone, GamepadId::kVendor24c6Productfafc},
    {0x24c6, 0xfafd, kXInputTypeNone, GamepadId::kVendor24c6Productfafd},
    {0x24c6, 0xfafe, kXInputTypeXbox360, GamepadId::kVendor24c6Productfafe},
    {0x2563, 0x0523, kXInputTypeNone, GamepadId::kVendor2563Product0523},
    {0x25f0, 0x83c1, kXInputTypeNone, GamepadId::kVendor25f0Product83c1},
    {0x25f0, 0xc121, kXInputTypeNone, GamepadId::kVendor25f0Productc121},
    {0x2717, 0x3144, kXInputTypeNone, GamepadId::kVendor2717Product3144},
    {0x2810, 0x0009, kXInputTypeNone, GamepadId::kVendor2810Product0009},
    {0x2836, 0x0001, kXInputTypeNone, GamepadId::kVendor2836Product0001},
    // Dracal/Raphnet technologies
    {0x289b, 0x0003, kXInputTypeNone, GamepadId::kDracalRaphnetProduct0003},
    {0x289b, 0x0005, kXInputTypeNone, GamepadId::kDracalRaphnetProduct0005},
    // Valve Software
    {0x28de, 0x1002, kXInputTypeNone, GamepadId::kValveProduct1002},
    {0x28de, 0x1042, kXInputTypeNone, GamepadId::kValveProduct1042},
    {0x28de, 0x1052, kXInputTypeNone, GamepadId::kValveProduct1052},
    {0x28de, 0x1071, kXInputTypeNone, GamepadId::kValveProduct1071},
    {0x28de, 0x1101, kXInputTypeNone, GamepadId::kValveProduct1101},
    {0x28de, 0x1102, kXInputTypeNone, GamepadId::kValveProduct1102},
    {0x28de, 0x1105, kXInputTypeNone, GamepadId::kValveProduct1105},
    {0x28de, 0x1106, kXInputTypeNone, GamepadId::kValveProduct1106},
    {0x28de, 0x1142, kXInputTypeNone, GamepadId::kValveProduct1142},
    {0x28de, 0x11fc, kXInputTypeNone, GamepadId::kValveProduct11fc},
    {0x28de, 0x11ff, kXInputTypeNone, GamepadId::kValveProduct11ff},
    {0x28de, 0x1201, kXInputTypeNone, GamepadId::kValveProduct1201},
    {0x28de, 0x1202, kXInputTypeNone, GamepadId::kValveProduct1202},
    {0x2c22, 0x2000, kXInputTypeNone, GamepadId::kVendor2c22Product2000},
    {0x2c22, 0x2300, kXInputTypeNone, GamepadId::kVendor2c22Product2300},
    {0x2c22, 0x2302, kXInputTypeNone, GamepadId::kVendor2c22Product2302},
    {0x2dc8, 0x1003, kXInputTypeNone, GamepadId::kEightBitdoProduct1003},
    {0x2dc8, 0x1080, kXInputTypeNone, GamepadId::kEightBitdoProduct1080},
    {0x2dc8, 0x2830, kXInputTypeNone, GamepadId::kEightBitdoProduct2830},
    {0x2dc8, 0x3000, kXInputTypeNone, GamepadId::kEightBitdoProduct3000},
    {0x2dc8, 0x3001, kXInputTypeNone, GamepadId::kEightBitdoProduct3001},
    {0x2dc8, 0x3820, kXInputTypeNone, GamepadId::kEightBitdoProduct3820},
    {0x2dc8, 0x9001, kXInputTypeNone, GamepadId::kEightBitdoProduct9001},
    {0x2dfa, 0x0001, kXInputTypeNone, GamepadId::kVendor2dfaProduct0001},
    {0x3767, 0x0101, kXInputTypeXbox, GamepadId::kVendor3767Product0101},
    {0x3820, 0x0009, kXInputTypeNone, GamepadId::kVendor3820Product0009},
    {0x4c50, 0x5453, kXInputTypeNone, GamepadId::kVendor4c50Product5453},
    {0x5347, 0x6d61, kXInputTypeNone, GamepadId::kVendor5347Product6d61},
    {0x6469, 0x6469, kXInputTypeNone, GamepadId::kVendor6469Product6469},
    // Prototype product Vendor ID
    {0x6666, 0x0667, kXInputTypeNone, GamepadId::kPrototypeVendorProduct0667},
    {0x6666, 0x8804, kXInputTypeNone, GamepadId::kPrototypeVendorProduct8804},
    {0x6666, 0x9401, kXInputTypeNone, GamepadId::kPrototypeVendorProduct9401},
    {0x6957, 0x746f, kXInputTypeNone, GamepadId::kVendor6957Product746f},
    {0x6978, 0x706e, kXInputTypeNone, GamepadId::kVendor6978Product706e},
    {0x8000, 0x1002, kXInputTypeNone, GamepadId::kVendor8000Product1002},
    {0x8888, 0x0308, kXInputTypeNone, GamepadId::kVendor8888Product0308},
    {0xf000, 0x0003, kXInputTypeNone, GamepadId::kVendorf000Product0003},
    {0xf000, 0x00f1, kXInputTypeNone, GamepadId::kVendorf000Product00f1},
    // Hama
    {0xf766, 0x0001, kXInputTypeNone, GamepadId::kHamaProduct0001},
    {0xf766, 0x0005, kXInputTypeNone, GamepadId::kHamaProduct0005},
};
constexpr size_t kGamepadInfoLength = base::size(kGamepadInfo);

bool CompareEntry(const GamepadInfo& a, const GamepadInfo& b) {
  return std::tie(a.vendor, a.product) < std::tie(b.vendor, b.product);
}

const GamepadInfo* GetGamepadInfo(uint16_t vendor, uint16_t product) {
  const GamepadInfo* begin = std::begin(kGamepadInfo);
  const GamepadInfo* end = std::end(kGamepadInfo);
  GamepadInfo target_entry = {vendor, product, kXInputTypeNone,
                              GamepadId::kUnknownGamepad};
  const GamepadInfo* it =
      std::lower_bound(begin, end, target_entry, CompareEntry);
  if (it == end || it->vendor != vendor || it->product != product)
    return nullptr;
  return it;
}

}  // namespace

// static
GamepadIdList& GamepadIdList::Get() {
  return g_singleton.Get();
}

GamepadIdList::GamepadIdList() {
  DCHECK(std::is_sorted(std::begin(kGamepadInfo), std::end(kGamepadInfo),
                        CompareEntry));
}

XInputType GamepadIdList::GetXInputType(uint16_t vendor_id,
                                        uint16_t product_id) const {
  const auto* entry = GetGamepadInfo(vendor_id, product_id);
  return entry ? entry->xtype : kXInputTypeNone;
}

GamepadId GamepadIdList::GetGamepadId(uint16_t vendor_id,
                                      uint16_t product_id) const {
  const auto* entry = GetGamepadInfo(vendor_id, product_id);
  return entry ? entry->id : GamepadId::kUnknownGamepad;
}

std::vector<std::tuple<uint16_t, uint16_t, XInputType, GamepadId>>
GamepadIdList::GetGamepadListForTesting() const {
  std::vector<std::tuple<uint16_t, uint16_t, XInputType, GamepadId>> gamepads;
  for (size_t i = 0; i < kGamepadInfoLength; ++i) {
    const auto& item = kGamepadInfo[i];
    gamepads.push_back(
        std::make_tuple(item.vendor, item.product, item.xtype, item.id));
  }
  return gamepads;
}

}  // namespace device
