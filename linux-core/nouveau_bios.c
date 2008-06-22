/*
 * Copyright (C) 2005-2006 Erik Waling
 * Copyright (C) 2006 Stephane Marchesin
 * Copyright (C) 2007-2008 Stuart Bennett
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <asm/byteorder.h>
#include "nouveau_bios.h"
#include "nouveau_drv.h"

/* returns true if it mismatches */
static bool nv_checksum(const uint8_t *data, unsigned int length)
{
	/* there's a few checksums in the BIOS, so here's a generic checking function */
	int i;
	uint8_t sum = 0;

	for (i = 0; i < length; i++)
		sum += data[i];

	if (sum)
		return true;

	return false;
}

static int nv_valid_bios(struct drm_device *dev, uint8_t *data)
{
	/* check for BIOS signature */
	if (!(data[0] == 0x55 && data[1] == 0xAA)) {
		DRM_ERROR("BIOS signature not found.\n");
		return 0;
	}

	if (nv_checksum(data, data[2] * 512)) {
		DRM_ERROR("BIOS checksum invalid.\n");
		return 1;
	}

	return 2;
}

static void nv_shadow_bios_rom(struct drm_device *dev, uint8_t *data)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int i;

	/* enable access to rom */
	NV_WRITE(NV04_PBUS_PCI_NV_20, NV04_PBUS_PCI_NV_20_ROM_SHADOW_DISABLED);

	/* This is also valid for pre-NV50, it just happened to be the only define already present. */
	for (i=0; i < NV50_PROM__ESIZE; i++) {
		/* Appearantly needed for a 6600GT/6800LE bug. */
		data[i] = DRM_READ8(dev_priv->mmio, NV50_PROM + i);
		data[i] = DRM_READ8(dev_priv->mmio, NV50_PROM + i);
		data[i] = DRM_READ8(dev_priv->mmio, NV50_PROM + i);
		data[i] = DRM_READ8(dev_priv->mmio, NV50_PROM + i);
		data[i] = DRM_READ8(dev_priv->mmio, NV50_PROM + i);
	}

	/* disable access to rom */
	NV_WRITE(NV04_PBUS_PCI_NV_20, NV04_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED);
}

static void nv_shadow_bios_ramin(struct drm_device *dev, uint8_t *data)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t old_bar0_pramin = 0;
	int i;

	/* Move the bios copy to the start of ramin? */
	if (dev_priv->card_type >= NV_50) {
		uint32_t vbios_vram = (NV_READ(0x619f04) & ~0xff) << 8;

		if (!vbios_vram)
			vbios_vram = (NV_READ(0x1700) << 16) + 0xf0000;

		old_bar0_pramin = NV_READ(0x1700);
		NV_WRITE(0x1700, vbios_vram >> 16);
	}

	for (i=0; i < NV50_PROM__ESIZE; i++)
		data[i] = DRM_READ8(dev_priv->mmio, NV04_PRAMIN + i);

	if (dev_priv->card_type >= NV_50)
		NV_WRITE(0x1700, old_bar0_pramin);
}

static bool nv_shadow_bios(struct drm_device *dev, uint8_t *data)
{
	nv_shadow_bios_rom(dev, data);
	if (nv_valid_bios(dev, data) == 2)
		return true;

	nv_shadow_bios_ramin(dev, data);
	if (nv_valid_bios(dev, data))
		return true;

	return false;
}

struct bit_entry {
	uint8_t id[2];
	uint16_t length;
	uint16_t offset;
};

static int parse_bit_C_tbl_entry(struct drm_device *dev, struct bios *bios, struct bit_entry *bitentry)
{
	/* offset + 8  (16 bits): PLL limits table pointer
	 *
	 * There's more in here, but that's unknown.
	 */

	if (bitentry->length < 10) {
		DRM_ERROR( "Do not understand BIT C table\n");
		return 0;
	}

	bios->pll_limit_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset + 8])));

	return 1;
}

static void parse_bit_structure(struct drm_device *dev, struct bios *bios, const uint16_t bitoffset)
{
	int entries = bios->data[bitoffset + 4];
	/* parse i first, I next (which needs C & M before it), and L before D */
	char parseorder[] = "iCMILDT";
	struct bit_entry bitentry;
	int i, j, offset;

	for (i = 0; i < sizeof(parseorder); i++) {
		for (j = 0, offset = bitoffset + 6; j < entries; j++, offset += 6) {
			bitentry.id[0] = bios->data[offset];
			bitentry.id[1] = bios->data[offset + 1];
			bitentry.length = le16_to_cpu(*((uint16_t *)&bios->data[offset + 2]));
			bitentry.offset = le16_to_cpu(*((uint16_t *)&bios->data[offset + 4]));

			if (bitentry.id[0] != parseorder[i])
				continue;

			switch (bitentry.id[0]) {
			case 'C':
				parse_bit_C_tbl_entry(dev, bios, &bitentry);
				break;
			}
		}
	}
}

static uint16_t findstr(uint8_t *data, int n, const uint8_t *str, int len)
{
	int i, j;

	for (i = 0; i <= (n - len); i++) {
		for (j = 0; j < len; j++)
			if (data[i + j] != str[j])
				break;
		if (j == len)
			return i;
	}

	return 0;
}

static void
read_dcb_i2c_entry(struct drm_device *dev, uint8_t dcb_version, uint16_t i2ctabptr, int index)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct bios *bios = &dev_priv->bios;
	uint8_t *i2ctable = &bios->data[i2ctabptr];
	uint8_t headerlen = 0;
	int i2c_entries = MAX_NUM_DCB_ENTRIES;
	int recordoffset = 0, rdofs = 1, wrofs = 0;

	if (!i2ctabptr)
		return;

	if (dcb_version >= 0x30) {
		if (i2ctable[0] != dcb_version) /* necessary? */
			DRM_ERROR(
				   "DCB I2C table version mismatch (%02X vs %02X)\n",
				   i2ctable[0], dcb_version);
		headerlen = i2ctable[1];
		i2c_entries = i2ctable[2];

		/* same address offset used for read and write for C51 and G80 */
		if (bios->chip_version == 0x51)
			rdofs = wrofs = 1;
		if (i2ctable[0] >= 0x40)
			rdofs = wrofs = 0;
	}
	/* it's your own fault if you call this function on a DCB 1.1 BIOS --
	 * the test below is for DCB 1.2
	 */
	if (dcb_version < 0x14) {
		recordoffset = 2;
		rdofs = 0;
		wrofs = 1;
	}

	if (index == 0xf)
		return;
	if (index > i2c_entries) {
		DRM_ERROR(
			   "DCB I2C index too big (%d > %d)\n",
			   index, i2ctable[2]);
		return;
	}
	if (i2ctable[headerlen + 4 * index + 3] == 0xff) {
		DRM_ERROR(
			   "DCB I2C entry invalid\n");
		return;
	}

	if (bios->chip_version == 0x51) {
		int port_type = i2ctable[headerlen + 4 * index + 3];

		if (port_type != 4)
			DRM_ERROR(
				   "DCB I2C table has port type %d\n", port_type);
	}
	if (i2ctable[0] >= 0x40) {
		int port_type = i2ctable[headerlen + 4 * index + 3];

		if (port_type != 5)
			DRM_ERROR(
				   "DCB I2C table has port type %d\n", port_type);
	}

	dev_priv->dcb_table.i2c_read[index] = i2ctable[headerlen + recordoffset + rdofs + 4 * index];
	dev_priv->dcb_table.i2c_write[index] = i2ctable[headerlen + recordoffset + wrofs + 4 * index];
}

static bool
parse_dcb_entry(struct drm_device *dev, int index, uint8_t dcb_version, uint16_t i2ctabptr, uint32_t conn, uint32_t conf)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct dcb_entry *entry = &dev_priv->dcb_table.entry[index];

	memset(entry, 0, sizeof (struct dcb_entry));

	entry->index = index;
	/* safe defaults for a crt */
	entry->type = 0;
	entry->i2c_index = 0;
	entry->heads = 1;
	entry->bus = 0;
	entry->location = LOC_ON_CHIP;
	entry->or = 1;
	entry->duallink_possible = false;

	if (dcb_version >= 0x20) {
		entry->type = conn & 0xf;
		entry->i2c_index = (conn >> 4) & 0xf;
		entry->heads = (conn >> 8) & 0xf;
		entry->bus = (conn >> 16) & 0xf;
		entry->location = (conn >> 20) & 0xf;
		entry->or = (conn >> 24) & 0xf;
		/* Normal entries consist of a single bit, but dual link has the
		 * adjacent more significant bit set too
		 */
		if ((1 << (ffs(entry->or) - 1)) * 3 == entry->or)
			entry->duallink_possible = true;

		switch (entry->type) {
		case DCB_OUTPUT_LVDS:
			{
			uint32_t mask;
			if (conf & 0x1)
				entry->lvdsconf.use_straps_for_mode = true;
			if (dcb_version < 0x22) {
				mask = ~0xd;
				/* both 0x4 and 0x8 show up in v2.0 tables; assume they mean
				 * the same thing, which is probably wrong, but might work */
				if (conf & 0x4 || conf & 0x8)
					entry->lvdsconf.use_power_scripts = true;
			} else {
				mask = ~0x5;
				if (conf & 0x4)
					entry->lvdsconf.use_power_scripts = true;
			}
			if (conf & mask) {
				DRM_ERROR(
					   "Unknown LVDS configuration bits, please report\n");
				/* cause output setting to fail, so message is seen */
				dev_priv->dcb_table.entries = 0;
				return false;
			}
			break;
			}
		case 0xe:
			/* weird type that appears on g80 mobile bios; nv driver treats it as a terminator */
			return false;
		}
		read_dcb_i2c_entry(dev, dcb_version, i2ctabptr, entry->i2c_index);
	} else if (dcb_version >= 0x14 ) {
		if (conn != 0xf0003f00 && conn != 0xf2247f10 && conn != 0xf2204001 && conn != 0xf2204301 && conn != 0xf2244311 && conn != 0xf2045f14 && conn != 0xf2205004 && conn != 0xf2208001 && conn != 0xf4204011 && conn != 0xf4208011 && conn != 0xf4248011) {
			DRM_ERROR(
				   "Unknown DCB 1.4 / 1.5 entry, please report\n");
			/* cause output setting to fail, so message is seen */
			dev_priv->dcb_table.entries = 0;
			return false;
		}
		/* most of the below is a "best guess" atm */
		entry->type = conn & 0xf;
		if (entry->type == 4) { /* digital */
			if (conn & 0x10)
				entry->type = DCB_OUTPUT_LVDS;
			else
				entry->type = DCB_OUTPUT_TMDS;
		}
		/* what's in bits 5-13? could be some brooktree/chrontel/philips thing, in tv case */
		entry->i2c_index = (conn >> 14) & 0xf;
		/* raw heads field is in range 0-1, so move to 1-2 */
		entry->heads = ((conn >> 18) & 0x7) + 1;
		entry->location = (conn >> 21) & 0xf;
		entry->bus = (conn >> 25) & 0x7;
		/* set or to be same as heads -- hopefully safe enough */
		entry->or = entry->heads;

		switch (entry->type) {
		case DCB_OUTPUT_LVDS:
			/* this is probably buried in conn's unknown bits */
			entry->lvdsconf.use_power_scripts = true;
			break;
		case DCB_OUTPUT_TMDS:
			/* invent a DVI-A output, by copying the fields of the DVI-D output
			 * reported to work by math_b on an NV20(!) */
			memcpy(&entry[1], &entry[0], sizeof(struct dcb_entry));
			entry[1].type = DCB_OUTPUT_ANALOG;
			dev_priv->dcb_table.entries++;
		}
		read_dcb_i2c_entry(dev, dcb_version, i2ctabptr, entry->i2c_index);
	} else if (dcb_version >= 0x12) {
		/* v1.2 tables normally have the same 5 entries, which are not
		 * specific to the card, so use the defaults for a crt */
		/* DCB v1.2 does have an I2C table that read_dcb_i2c_table can handle, but cards
		 * exist (seen on nv11) where the pointer to the table points to the wrong
		 * place, so for now, we rely on the indices parsed in parse_bmp_structure
		 */
		entry->i2c_index = dev_priv->bios.legacy.i2c_indices.crt;
	} else { /* pre DCB / v1.1 - use the safe defaults for a crt */
		DRM_ERROR(
			   "No information in BIOS output table; assuming a CRT output exists\n");
		entry->i2c_index = dev_priv->bios.legacy.i2c_indices.crt;
	}

	if (entry->type == DCB_OUTPUT_LVDS && dev_priv->bios.fp.strapping != 0xff)
		entry->lvdsconf.use_straps_for_mode = true;

	dev_priv->dcb_table.entries++;

	return true;
}

static void merge_like_dcb_entries(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	/* DCB v2.0 lists each output combination separately.
	 * Here we merge compatible entries to have fewer outputs, with more options
	 */
	int i, newentries = 0;

	for (i = 0; i < dev_priv->dcb_table.entries; i++) {
		struct dcb_entry *ient = &dev_priv->dcb_table.entry[i];
		int j;

		for (j = i + 1; j < dev_priv->dcb_table.entries; j++) {
			struct dcb_entry *jent = &dev_priv->dcb_table.entry[j];

			if (jent->type == 100) /* already merged entry */
				continue;

			/* merge heads field when all other fields the same */
			if (jent->i2c_index == ient->i2c_index && jent->type == ient->type && jent->location == ient->location && jent->or == ient->or) {
				DRM_INFO(
					   "Merging DCB entries %d and %d\n", i, j);
				ient->heads |= jent->heads;
				jent->type = 100; /* dummy value */
			}
		}
	}

	/* Compact entries merged into others out of dcb_table */
	for (i = 0; i < dev_priv->dcb_table.entries; i++) {
		if ( dev_priv->dcb_table.entry[i].type == 100 )
			continue;

		if (newentries != i)
			memcpy(&dev_priv->dcb_table.entry[newentries], &dev_priv->dcb_table.entry[i], sizeof(struct dcb_entry));
		newentries++;
	}

	dev_priv->dcb_table.entries = newentries;
}

static unsigned int parse_dcb_table(struct drm_device *dev, struct bios *bios)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint16_t dcbptr, i2ctabptr = 0;
	uint8_t *dcbtable;
	uint8_t dcb_version, headerlen = 0x4, entries = MAX_NUM_DCB_ENTRIES;
	bool configblock = true;
	int recordlength = 8, confofs = 4;
	int i;

	dev_priv->dcb_table.entries = 0;

	/* get the offset from 0x36 */
	dcbptr = le16_to_cpu(*(uint16_t *)&bios->data[0x36]);

	if (dcbptr == 0x0) {
		DRM_ERROR(
			   "No Display Configuration Block pointer found\n");
		/* this situation likely means a really old card, pre DCB, so we'll add the safe CRT entry */
		parse_dcb_entry(dev, 0, 0, 0, 0, 0);
		return 1;
	}

	dcbtable = &bios->data[dcbptr];

	/* get DCB version */
	dcb_version = dcbtable[0];
	DRM_INFO(
		   "Display Configuration Block version %d.%d found\n",
		   dcb_version >> 4, dcb_version & 0xf);

	if (dcb_version >= 0x20) { /* NV17+ */
		uint32_t sig;

		if (dcb_version >= 0x30) { /* NV40+ */
			headerlen = dcbtable[1];
			entries = dcbtable[2];
			recordlength = dcbtable[3];
			i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[4]);
			sig = le32_to_cpu(*(uint32_t *)&dcbtable[6]);

			DRM_INFO(
				   "DCB header length %d, with %d possible entries\n",
				   headerlen, entries);
		} else {
			i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[2]);
			sig = le32_to_cpu(*(uint32_t *)&dcbtable[4]);
			headerlen = 8;
		}

		if (sig != 0x4edcbdcb) {
			DRM_ERROR(
				   "Bad Display Configuration Block signature (%08X)\n", sig);
			return 0;
		}
	} else if (dcb_version >= 0x14) { /* some NV15/16, and NV11+ */
		char sig[8];

		memset(sig, 0, 8);
		strncpy(sig, (char *)&dcbtable[-7], 7);
		i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[2]);
		recordlength = 10;
		confofs = 6;

		if (strcmp(sig, "DEV_REC")) {
			DRM_ERROR(
				   "Bad Display Configuration Block signature (%s)\n", sig);
			return 0;
		}
	} else if (dcb_version >= 0x12) { /* some NV6/10, and NV15+ */
		i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[2]);
		configblock = false;
	} else {	/* NV5+, maybe NV4 */
		/* DCB 1.1 seems to be quite unhelpful - we'll just add the safe CRT entry */
		parse_dcb_entry(dev, 0, dcb_version, 0, 0, 0);
		return 1;
	}

	if (entries >= MAX_NUM_DCB_ENTRIES)
		entries = MAX_NUM_DCB_ENTRIES;

	for (i = 0; i < entries; i++) {
		uint32_t connection, config = 0;

		connection = le32_to_cpu(*(uint32_t *)&dcbtable[headerlen + recordlength * i]);
		if (configblock)
			config = le32_to_cpu(*(uint32_t *)&dcbtable[headerlen + confofs + recordlength * i]);

		/* Should we allow discontinuous DCBs? Certainly DCB I2C tables can be discontinuous */
		if ((connection & 0x0000000f) == 0x0000000f) /* end of records */
			break;
		if (connection == 0x00000000) /* seen on an NV11 with DCB v1.5 */
			break;

		DRM_INFO("Raw DCB entry %d: %08x %08x\n", i, connection, config);
		if (!parse_dcb_entry(dev, dev_priv->dcb_table.entries, dcb_version, i2ctabptr, connection, config))
			break;
	}

	merge_like_dcb_entries(dev);

	return dev_priv->dcb_table.entries;
}

int nouveau_parse_bios(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	const uint8_t bit_signature[] = { 'B', 'I', 'T' };
	int offset;

	dev_priv->bios.data = kzalloc(NV50_PROM__ESIZE, GFP_KERNEL);
	if (!dev_priv->bios.data)
		return -ENOMEM;

	if (!nv_shadow_bios(dev, dev_priv->bios.data))
		return -EINVAL;

	dev_priv->bios.length = dev_priv->bios.data[2] * 512;
	if (dev_priv->bios.length > NV50_PROM__ESIZE)
		dev_priv->bios.length = NV50_PROM__ESIZE;

	if ((offset = findstr(dev_priv->bios.data, dev_priv->bios.length, bit_signature, sizeof(bit_signature)))) {
		DRM_INFO("BIT BIOS found\n");
		parse_bit_structure(dev, &dev_priv->bios, offset + 4);
	} else {
		DRM_ERROR("BIT BIOS not found\n");
		return -EINVAL;
	}

	if (parse_dcb_table(dev, &dev_priv->bios))
		DRM_INFO("Found %d entries in DCB\n", dev_priv->dcb_table.entries);

	return 0;
}

/* temporary */
#define NV_RAMDAC_NVPLL			0x00680500
#define NV_RAMDAC_MPLL			0x00680504
#define NV_RAMDAC_VPLL			0x00680508
#	define NV_RAMDAC_PLL_COEFF_MDIV			0x000000FF
#	define NV_RAMDAC_PLL_COEFF_NDIV			0x0000FF00
#	define NV_RAMDAC_PLL_COEFF_PDIV			0x00070000
#	define NV30_RAMDAC_ENABLE_VCO2			(1 << 7)
#define NV_RAMDAC_VPLL2			0x00680520

bool get_pll_limits(struct drm_device *dev, uint32_t limit_match, struct pll_lims *pll_lim)
{
	/* PLL limits table
	 *
	 * Version 0x10: NV31
	 * One byte header (version), one record of 24 bytes
	 * Version 0x11: NV36 - Not implemented
	 * Seems to have same record style as 0x10, but 3 records rather than 1
	 * Version 0x20: Found on Geforce 6 cards
	 * Trivial 4 byte BIT header. 31 (0x1f) byte record length
	 * Version 0x21: Found on Geforce 7, 8 and some Geforce 6 cards
	 * 5 byte header, fifth byte of unknown purpose. 35 (0x23) byte record
	 * length in general, some (integrated) have an extra configuration byte
	 */

	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct bios *bios = &dev_priv->bios;
	uint8_t pll_lim_ver = 0, headerlen = 0, recordlen = 0, entries = 0;
	int pllindex = 0;
	uint32_t crystal_strap_mask, crystal_straps;

	if (!bios->pll_limit_tbl_ptr) {
		if (bios->chip_version >= 0x40 || bios->chip_version == 0x31 || bios->chip_version == 0x36) {
			DRM_ERROR("Pointer to PLL limits table invalid\n");
			return false;
		}
	} else {
		pll_lim_ver = bios->data[bios->pll_limit_tbl_ptr];

		DRM_INFO("Found PLL limits table version 0x%X\n", pll_lim_ver);
	}

	crystal_strap_mask = 1 << 6;
	/* open coded pNv->twoHeads test */
	if (bios->chip_version > 0x10 && bios->chip_version != 0x15 &&
		bios->chip_version != 0x1a && bios->chip_version != 0x20)
		crystal_strap_mask |= 1 << 22;
	crystal_straps = NV_READ(NV50_PEXTDEV + 0x0) & crystal_strap_mask;

	switch (pll_lim_ver) {
	/* we use version 0 to indicate a pre limit table bios (single stage pll)
	 * and load the hard coded limits instead */
	case 0:
		break;
	case 0x10:
	case 0x11: /* strictly v0x11 has 3 entries, but the last two don't seem to get used */
		headerlen = 1;
		recordlen = 0x18;
		entries = 1;
		pllindex = 0;
		break;
	case 0x20:
	case 0x21:
		headerlen = bios->data[bios->pll_limit_tbl_ptr + 1];
		recordlen = bios->data[bios->pll_limit_tbl_ptr + 2];
		entries = bios->data[bios->pll_limit_tbl_ptr + 3];
		break;
	default:
		DRM_ERROR("PLL limits table revision not currently supported\n");
		return false;
	}

	/* initialize all members to zero */
	memset(pll_lim, 0, sizeof(struct pll_lims));

	if (pll_lim_ver == 0x10 || pll_lim_ver == 0x11) {
		uint16_t plloffs = bios->pll_limit_tbl_ptr + headerlen + recordlen * pllindex;

		pll_lim->vco1.minfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs])));
		pll_lim->vco1.maxfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 4])));
		pll_lim->vco2.minfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 8])));
		pll_lim->vco2.maxfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 12])));
		pll_lim->vco1.min_inputfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 16])));
		pll_lim->vco2.min_inputfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 20])));
		pll_lim->vco1.max_inputfreq = pll_lim->vco2.max_inputfreq = INT_MAX;

		/* these values taken from nv30/31/36 */
		pll_lim->vco1.min_n = 0x1;
		if (bios->chip_version == 0x36)
			pll_lim->vco1.min_n = 0x5;
		pll_lim->vco1.max_n = 0xff;
		pll_lim->vco1.min_m = 0x1;
		pll_lim->vco1.max_m = 0xd;
		pll_lim->vco2.min_n = 0x4;
		/* on nv30, 31, 36 (i.e. all cards with two stage PLLs with this
		 * table version (apart from nv35)), N2 is compared to
		 * maxN2 (0x46) and 10 * maxM2 (0x4), so set maxN2 to 0x28 and
		 * save a comparison
		 */
		pll_lim->vco2.max_n = 0x28;
		if (bios->chip_version == 0x30 || bios->chip_version == 0x35)
		       /* only 5 bits available for N2 on nv30/35 */
			pll_lim->vco2.max_n = 0x1f;
		pll_lim->vco2.min_m = 0x1;
		pll_lim->vco2.max_m = 0x4;
	} else if (pll_lim_ver) {	/* ver 0x20, 0x21 */
		uint16_t plloffs = bios->pll_limit_tbl_ptr + headerlen;
		uint32_t reg = 0; /* default match */
		int i;

		/* first entry is default match, if nothing better. warn if reg field nonzero */
		if (le32_to_cpu(*((uint32_t *)&bios->data[plloffs])))
			DRM_ERROR("Default PLL limit entry has non-zero register field\n");

		if (limit_match > MAX_PLL_TYPES)
			/* we've been passed a reg as the match */
			reg = limit_match;
		else /* limit match is a pll type */
			for (i = 1; i < entries && !reg; i++) {
				uint32_t cmpreg = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + recordlen * i])));

				if (limit_match == NVPLL && (cmpreg == NV_RAMDAC_NVPLL || cmpreg == 0x4000))
					reg = cmpreg;
				if (limit_match == MPLL && (cmpreg == NV_RAMDAC_MPLL || cmpreg == 0x4020))
					reg = cmpreg;
				if (limit_match == VPLL1 && (cmpreg == NV_RAMDAC_VPLL || cmpreg == 0x4010))
					reg = cmpreg;
				if (limit_match == VPLL2 && (cmpreg == NV_RAMDAC_VPLL2 || cmpreg == 0x4018))
					reg = cmpreg;
			}

		for (i = 1; i < entries; i++)
			if (le32_to_cpu(*((uint32_t *)&bios->data[plloffs + recordlen * i])) == reg) {
				pllindex = i;
				break;
			}

		plloffs += recordlen * pllindex;

		DRM_INFO("Loading PLL limits for reg 0x%08x\n", pllindex ? reg : 0);

		/* frequencies are stored in tables in MHz, kHz are more useful, so we convert */

		/* What output frequencies can each VCO generate? */
		pll_lim->vco1.minfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 4]))) * 1000;
		pll_lim->vco1.maxfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 6]))) * 1000;
		pll_lim->vco2.minfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 8]))) * 1000;
		pll_lim->vco2.maxfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 10]))) * 1000;

		/* What input frequencies do they accept (past the m-divider)? */
		pll_lim->vco1.min_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 12]))) * 1000;
		pll_lim->vco2.min_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 14]))) * 1000;
		pll_lim->vco1.max_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 16]))) * 1000;
		pll_lim->vco2.max_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 18]))) * 1000;

		/* What values are accepted as multiplier and divider? */
		pll_lim->vco1.min_n = bios->data[plloffs + 20];
		pll_lim->vco1.max_n = bios->data[plloffs + 21];
		pll_lim->vco1.min_m = bios->data[plloffs + 22];
		pll_lim->vco1.max_m = bios->data[plloffs + 23];
		pll_lim->vco2.min_n = bios->data[plloffs + 24];
		pll_lim->vco2.max_n = bios->data[plloffs + 25];
		pll_lim->vco2.min_m = bios->data[plloffs + 26];
		pll_lim->vco2.max_m = bios->data[plloffs + 27];

		pll_lim->unk1c = bios->data[plloffs + 28];
		pll_lim->max_log2p_bias = bios->data[plloffs + 29];
		pll_lim->log2p_bias = bios->data[plloffs + 30];

		if (recordlen > 0x22)
			pll_lim->refclk = le32_to_cpu(*((uint32_t *)&bios->data[plloffs + 31]));

		if (recordlen > 0x23)
			if (bios->data[plloffs + 35])
				DRM_ERROR("Bits set in PLL configuration byte (%x)\n", bios->data[plloffs + 35]);

		/* C51 special not seen elsewhere */
		/*if (bios->chip_version == 0x51 && !pll_lim->refclk) {
			uint32_t sel_clk = nv32_rd(pScrn, NV_RAMDAC_SEL_CLK);

			if (((limit_match == NV_RAMDAC_VPLL || limit_match == VPLL1) && sel_clk & 0x20) ||
			    ((limit_match == NV_RAMDAC_VPLL2 || limit_match == VPLL2) && sel_clk & 0x80)) {
				if (nv_idx_port_rd(pScrn, CRTC_INDEX_COLOR, NV_VGA_CRTCX_REVISION) < 0xa3)
					pll_lim->refclk = 200000;
				else
					pll_lim->refclk = 25000;
			}
		}*/
	}

	/* By now any valid limit table ought to have set a max frequency for
	 * vco1, so if it's zero it's either a pre limit table bios, or one
	 * with an empty limit table (seen on nv18)
	 */
	if (!pll_lim->vco1.maxfreq) {
		pll_lim->vco1.minfreq = bios->fminvco;
		pll_lim->vco1.maxfreq = bios->fmaxvco;
		pll_lim->vco1.min_inputfreq = 0;
		pll_lim->vco1.max_inputfreq = INT_MAX;
		pll_lim->vco1.min_n = 0x1;
		pll_lim->vco1.max_n = 0xff;
		pll_lim->vco1.min_m = 0x1;
		if (crystal_straps == 0) {
			/* nv05 does this, nv11 doesn't, nv10 unknown */
			if (bios->chip_version < 0x11)
				pll_lim->vco1.min_m = 0x7;
			pll_lim->vco1.max_m = 0xd;
		} else {
			if (bios->chip_version < 0x11)
				pll_lim->vco1.min_m = 0x8;
			pll_lim->vco1.max_m = 0xe;
		}
	}

	if (!pll_lim->refclk)
		switch (crystal_straps) {
		case 0:
			pll_lim->refclk = 13500;
			break;
		case (1 << 6):
			pll_lim->refclk = 14318;
			break;
		case (1 << 22):
			pll_lim->refclk = 27000;
			break;
		case (1 << 22 | 1 << 6):
			pll_lim->refclk = 25000;
			break;
		}

#if 1 /* for easy debugging */
	DRM_INFO("pll.vco1.minfreq: %d\n", pll_lim->vco1.minfreq);
	DRM_INFO("pll.vco1.maxfreq: %d\n", pll_lim->vco1.maxfreq);
	DRM_INFO("pll.vco2.minfreq: %d\n", pll_lim->vco2.minfreq);
	DRM_INFO("pll.vco2.maxfreq: %d\n", pll_lim->vco2.maxfreq);

	DRM_INFO("pll.vco1.min_inputfreq: %d\n", pll_lim->vco1.min_inputfreq);
	DRM_INFO("pll.vco1.max_inputfreq: %d\n", pll_lim->vco1.max_inputfreq);
	DRM_INFO("pll.vco2.min_inputfreq: %d\n", pll_lim->vco2.min_inputfreq);
	DRM_INFO("pll.vco2.max_inputfreq: %d\n", pll_lim->vco2.max_inputfreq);

	DRM_INFO("pll.vco1.min_n: %d\n", pll_lim->vco1.min_n);
	DRM_INFO("pll.vco1.max_n: %d\n", pll_lim->vco1.max_n);
	DRM_INFO("pll.vco1.min_m: %d\n", pll_lim->vco1.min_m);
	DRM_INFO("pll.vco1.max_m: %d\n", pll_lim->vco1.max_m);
	DRM_INFO("pll.vco2.min_n: %d\n", pll_lim->vco2.min_n);
	DRM_INFO("pll.vco2.max_n: %d\n", pll_lim->vco2.max_n);
	DRM_INFO("pll.vco2.min_m: %d\n", pll_lim->vco2.min_m);
	DRM_INFO("pll.vco2.max_m: %d\n", pll_lim->vco2.max_m);

	DRM_INFO("pll.unk1c: %d\n", pll_lim->unk1c);
	DRM_INFO("pll.max_log2p_bias: %d\n", pll_lim->max_log2p_bias);
	DRM_INFO("pll.log2p_bias: %d\n", pll_lim->log2p_bias);

	DRM_INFO("pll.refclk: %d\n", pll_lim->refclk);
#endif

	return true;
}
