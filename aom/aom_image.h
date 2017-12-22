/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\file
 * \brief Describes the aom image descriptor and associated operations
 *
 */
#ifndef AOM_AOM_IMAGE_H_
#define AOM_AOM_IMAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging
 * fields to structures
 */
#define AOM_IMAGE_ABI_VERSION (4) /**<\hideinitializer*/

#define AOM_IMG_FMT_PLANAR 0x100       /**< Image is a planar format. */
#define AOM_IMG_FMT_UV_FLIP 0x200      /**< V plane precedes U in memory. */
#define AOM_IMG_FMT_HAS_ALPHA 0x400    /**< Image has an alpha channel. */
#define AOM_IMG_FMT_HIGHBITDEPTH 0x800 /**< Image uses 16bit framebuffer. */

#include "./aom_config.h"

/*!\brief List of supported image formats */
typedef enum aom_img_fmt {
  AOM_IMG_FMT_NONE,
  AOM_IMG_FMT_RGB24,     /**< 24 bit per pixel packed RGB */
  AOM_IMG_FMT_RGB32,     /**< 32 bit per pixel packed 0RGB */
  AOM_IMG_FMT_RGB565,    /**< 16 bit per pixel, 565 */
  AOM_IMG_FMT_RGB555,    /**< 16 bit per pixel, 555 */
  AOM_IMG_FMT_UYVY,      /**< UYVY packed YUV */
  AOM_IMG_FMT_YUY2,      /**< YUYV packed YUV */
  AOM_IMG_FMT_YVYU,      /**< YVYU packed YUV */
  AOM_IMG_FMT_BGR24,     /**< 24 bit per pixel packed BGR */
  AOM_IMG_FMT_RGB32_LE,  /**< 32 bit packed BGR0 */
  AOM_IMG_FMT_ARGB,      /**< 32 bit packed ARGB, alpha=255 */
  AOM_IMG_FMT_ARGB_LE,   /**< 32 bit packed BGRA, alpha=255 */
  AOM_IMG_FMT_RGB565_LE, /**< 16 bit per pixel, gggbbbbb rrrrrggg */
  AOM_IMG_FMT_RGB555_LE, /**< 16 bit per pixel, gggbbbbb 0rrrrrgg */
  AOM_IMG_FMT_YV12 =
      AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP | 1, /**< planar YVU */
  AOM_IMG_FMT_I420 = AOM_IMG_FMT_PLANAR | 2,
  AOM_IMG_FMT_AOMYV12 = AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP |
                        3, /** < planar 4:2:0 format with aom color space */
  AOM_IMG_FMT_AOMI420 = AOM_IMG_FMT_PLANAR | 4,
  AOM_IMG_FMT_I422 = AOM_IMG_FMT_PLANAR | 5,
  AOM_IMG_FMT_I444 = AOM_IMG_FMT_PLANAR | 6,
  AOM_IMG_FMT_I440 = AOM_IMG_FMT_PLANAR | 7,
  AOM_IMG_FMT_444A = AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_HAS_ALPHA | 6,
  AOM_IMG_FMT_I42016 = AOM_IMG_FMT_I420 | AOM_IMG_FMT_HIGHBITDEPTH,
  AOM_IMG_FMT_I42216 = AOM_IMG_FMT_I422 | AOM_IMG_FMT_HIGHBITDEPTH,
  AOM_IMG_FMT_I44416 = AOM_IMG_FMT_I444 | AOM_IMG_FMT_HIGHBITDEPTH,
  AOM_IMG_FMT_I44016 = AOM_IMG_FMT_I440 | AOM_IMG_FMT_HIGHBITDEPTH
} aom_img_fmt_t; /**< alias for enum aom_img_fmt */

#if CONFIG_CICP
/*!\brief List of supported color primaries */
typedef enum aom_color_primaries {
  AOM_CICP_CP_RESERVED_0 = 0,  /**< For future use */
  AOM_CICP_CP_BT_709 = 1,      /**< BT.709 */
  AOM_CICP_CP_UNSPECIFIED = 2, /**< Unspecified */
  AOM_CICP_CP_RESERVED_3 = 3,  /**< For future use */
  AOM_CICP_CP_BT_470_M = 4,    /**< BT.470 System M (historical) */
  AOM_CICP_CP_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
  AOM_CICP_CP_BT_601 = 6,      /**< BT.601 */
  AOM_CICP_CP_SMPTE_240 = 7,   /**< SMPTE 240 */
  AOM_CICP_CP_GENERIC_FILM =
      8, /**< Generic film (color filters using illuminant C) */
  AOM_CICP_CP_BT_2020 = 9,      /**< BT.2020, BT.2100 */
  AOM_CICP_CP_XYZ = 10,         /**< SMPTE 428 (CIE 1921 XYZ) */
  AOM_CICP_CP_SMPTE_431 = 11,   /**< SMPTE RP 431-2 */
  AOM_CICP_CP_SMPTE_432 = 12,   /**< SMPTE EG 432-1  */
  AOM_CICP_CP_RESERVED_13 = 13, /**< For future use (values 13 - 21)  */
  AOM_CICP_CP_EBU_3213 = 22,    /**< EBU Tech. 3213-E  */
  AOM_CICP_CP_RESERVED_23 = 23  /**< For future use (values 23 - 255)  */
} aom_color_primaries_t;        /**< alias for enum aom_color_primaries */

/*!\brief List of supported transfer functions */
typedef enum aom_transfer_characteristics {
  AOM_CICP_TC_RESERVED_0 = 0,  /**< For future use */
  AOM_CICP_TC_BT_709 = 1,      /**< BT.709 */
  AOM_CICP_TC_UNSPECIFIED = 2, /**< Unspecified */
  AOM_CICP_TC_RESERVED_3 = 3,  /**< For future use */
  AOM_CICP_TC_BT_470_M = 4,    /**< BT.470 System M (historical)  */
  AOM_CICP_TC_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
  AOM_CICP_TC_BT_601 = 6,      /**< BT.601 */
  AOM_CICP_TC_SMPTE_240 = 7,   /**< SMPTE 240 M */
  AOM_CICP_TC_LINEAR = 8,      /**< Linear */
  AOM_CICP_TC_LOG_100 = 9,     /**< Logarithmic (100 : 1 range) */
  AOM_CICP_TC_LOG_100_SQRT10 =
      10,                     /**< Logarithmic (100 * Sqrt(10) : 1 range) */
  AOM_CICP_TC_IEC_61966 = 11, /**< IEC 61966-2-4 */
  AOM_CICP_TC_BT_1361 = 12,   /**< BT.1361 */
  AOM_CICP_TC_SRGB = 13,      /**< sRGB or sYCC*/
  AOM_CICP_TC_BT_2020_10_BIT = 14, /**< BT.2020 10-bit systems */
  AOM_CICP_TC_BT_2020_12_BIT = 15, /**< BT.2020 12-bit systems */
  AOM_CICP_TC_SMPTE_2084 = 16,     /**< SMPTE ST 2084, ITU BT.2100 PQ */
  AOM_CICP_TC_SMPTE_428 = 17,      /**< SMPTE ST 428 */
  AOM_CICP_TC_HLG = 18,            /**< BT.2100 HLG, ARIB STD-B67 */
  AOM_CICP_TC_RESERVED_19 = 19     /**< For future use (values 19-255) */
} aom_transfer_characteristics_t;  /**< alias for enum aom_transfer_function */

/*!\brief List of supported matrix coefficients */
typedef enum aom_matrix_coefficients {
  AOM_CICP_MC_IDENTITY = 0,    /**< Identity matrix */
  AOM_CICP_MC_BT_709 = 1,      /**< BT.709 */
  AOM_CICP_MC_UNSPECIFIED = 2, /**< Unspecified */
  AOM_CICP_MC_RESERVED_3 = 3,  /**< For future use */
  AOM_CICP_MC_FCC = 4,         /**< US FCC 73.628 */
  AOM_CICP_MC_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
  AOM_CICP_MC_BT_601 = 6,      /**< BT.601 */
  AOM_CICP_MC_SMPTE_240 = 7,   /**< SMPTE 240 M */
  AOM_CICP_MC_SMPTE_YCGCO = 8, /**< YCgCo */
  AOM_CICP_MC_BT_2020_NCL =
      9, /**< BT.2020 non-constant luminance, BT.2100 YCbCr  */
  AOM_CICP_MC_BT_2020_CL = 10, /**< BT.2020 constant luminance */
  AOM_CICP_MC_SMPTE_2085 = 11, /**< SMPTE ST 2085 YDzDx */
  AOM_CICP_MC_CHROMAT_NCL =
      12, /**< Chromaticity-derived non-constant luminance */
  AOM_CICP_MC_CHROMAT_CL = 13, /**< Chromaticity-derived constant luminance */
  AOM_CICP_MC_ICTCP = 14,      /**< BT.2100 ICtCp */
  AOM_CICP_MC_RESERVED_15 = 15 /**< For future use (values 15-255)  */
} aom_matrix_coefficients_t;
#else
/*!\brief List of supported color spaces */
typedef enum aom_color_space {
  AOM_CS_UNKNOWN = 0,     /**< Unknown */
  AOM_CS_BT_601 = 1,      /**< BT.601 */
  AOM_CS_BT_709 = 2,      /**< BT.709 */
  AOM_CS_SMPTE_170 = 3,   /**< SMPTE.170 */
  AOM_CS_SMPTE_240 = 4,   /**< SMPTE.240 */
  AOM_CS_BT_2020_NCL = 5, /**< BT.2020 non-constant luminance (BT.2100) */
  AOM_CS_BT_2020_CL = 6,  /**< BT.2020 constant luminance */
  AOM_CS_SRGB = 7,        /**< sRGB */
  AOM_CS_ICTCP = 8,       /**< ICtCp, ITU-R BT.2100 */
  AOM_CS_MONOCHROME = 9,  /**< Monochrome */
  AOM_CS_RESERVED = 10    /**< Values 10..31 are reserved */
} aom_color_space_t;      /**< alias for enum aom_color_space */

/*!\brief List of supported transfer functions */
typedef enum aom_transfer_function {
  AOM_TF_UNKNOWN = 0,         /**< Unknown */
  AOM_TF_BT_709 = 1,          /**< BT.709 */
  AOM_TF_PQ = 2,              /**< PQ TF BT.2100 / ST.2084 */
  AOM_TF_HLG = 3,             /**< Hybrid Log-Gamma */
  AOM_TF_RESERVED = 4         /**< Values 4..31 are reserved */
} aom_transfer_function_t;    /**< alias for enum aom_transfer_function */
#endif

/*!\brief List of supported color range */
typedef enum aom_color_range {
  AOM_CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
  AOM_CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
} aom_color_range_t;       /**< alias for enum aom_color_range */

/*!\brief List of chroma sample positions */
typedef enum aom_chroma_sample_position {
  AOM_CSP_UNKNOWN = 0,          /**< Unknown */
  AOM_CSP_VERTICAL = 1,         /**< Horizontally co-located with luma(0, 0)*/
                                /**< sample, between two vertical samples */
  AOM_CSP_COLOCATED = 2,        /**< Co-located with luma(0, 0) sample */
  AOM_CSP_RESERVED = 3          /**< Reserved value */
} aom_chroma_sample_position_t; /**< alias for enum aom_transfer_function */

/**\brief Image Descriptor */
typedef struct aom_image {
  aom_img_fmt_t fmt; /**< Image Format */
#if CONFIG_CICP
  aom_color_primaries_t cp;          /**< CICP Color Primaries */
  aom_transfer_characteristics_t tc; /**< CICP Transfer Characteristics */
  aom_matrix_coefficients_t mc;      /**< CICP Matrix Coefficients */
#else
  aom_color_space_t cs;       /**< Color Space */
  aom_transfer_function_t tf; /**< transfer function */
#endif
  aom_chroma_sample_position_t csp; /**< chroma sample position */
  aom_color_range_t range;          /**< Color Range */

  /* Image storage dimensions */
  unsigned int w;         /**< Stored image width */
  unsigned int h;         /**< Stored image height */
  unsigned int bit_depth; /**< Stored image bit-depth */

  /* Image display dimensions */
  unsigned int d_w; /**< Displayed image width */
  unsigned int d_h; /**< Displayed image height */

  /* Image intended rendering dimensions */
  unsigned int r_w; /**< Intended rendering image width */
  unsigned int r_h; /**< Intended rendering image height */

  /* Chroma subsampling info */
  unsigned int x_chroma_shift; /**< subsampling order, X */
  unsigned int y_chroma_shift; /**< subsampling order, Y */

/* Image data pointers. */
#define AOM_PLANE_PACKED 0  /**< To be used for all packed formats */
#define AOM_PLANE_Y 0       /**< Y (Luminance) plane */
#define AOM_PLANE_U 1       /**< U (Chroma) plane */
#define AOM_PLANE_V 2       /**< V (Chroma) plane */
#define AOM_PLANE_ALPHA 3   /**< A (Transparency) plane */
  unsigned char *planes[4]; /**< pointer to the top left pixel for each plane */
  int stride[4];            /**< stride between rows for each plane */

  int bps; /**< bits per sample (for packed formats) */

  /*!\brief The following member may be set by the application to associate
   * data with this image.
   */
  void *user_priv;

  /* The following members should be treated as private. */
  unsigned char *img_data; /**< private */
  int img_data_owner;      /**< private */
  int self_allocd;         /**< private */

  void *fb_priv; /**< Frame buffer data associated with the image. */
} aom_image_t;   /**< alias for struct aom_image */

/**\brief Representation of a rectangle on a surface */
typedef struct aom_image_rect {
  unsigned int x;   /**< leftmost column */
  unsigned int y;   /**< topmost row */
  unsigned int w;   /**< width */
  unsigned int h;   /**< height */
} aom_image_rect_t; /**< alias for struct aom_image_rect */

/*!\brief Open a descriptor, allocating storage for the underlying image
 *
 * Returns a descriptor for storing an image of the given format. The
 * storage for the descriptor is allocated on the heap.
 *
 * \param[in]    img       Pointer to storage for descriptor. If this parameter
 *                         is NULL, the storage for the descriptor will be
 *                         allocated on the heap.
 * \param[in]    fmt       Format for the image
 * \param[in]    d_w       Width of the image
 * \param[in]    d_h       Height of the image
 * \param[in]    align     Alignment, in bytes, of the image buffer and
 *                         each row in the image(stride).
 *
 * \return Returns a pointer to the initialized image descriptor. If the img
 *         parameter is non-null, the value of the img parameter will be
 *         returned.
 */
aom_image_t *aom_img_alloc(aom_image_t *img, aom_img_fmt_t fmt,
                           unsigned int d_w, unsigned int d_h,
                           unsigned int align);

/*!\brief Open a descriptor, using existing storage for the underlying image
 *
 * Returns a descriptor for storing an image of the given format. The
 * storage for descriptor has been allocated elsewhere, and a descriptor is
 * desired to "wrap" that storage.
 *
 * \param[in]    img       Pointer to storage for descriptor. If this parameter
 *                         is NULL, the storage for the descriptor will be
 *                         allocated on the heap.
 * \param[in]    fmt       Format for the image
 * \param[in]    d_w       Width of the image
 * \param[in]    d_h       Height of the image
 * \param[in]    align     Alignment, in bytes, of each row in the image.
 * \param[in]    img_data  Storage to use for the image
 *
 * \return Returns a pointer to the initialized image descriptor. If the img
 *         parameter is non-null, the value of the img parameter will be
 *         returned.
 */
aom_image_t *aom_img_wrap(aom_image_t *img, aom_img_fmt_t fmt, unsigned int d_w,
                          unsigned int d_h, unsigned int align,
                          unsigned char *img_data);

/*!\brief Set the rectangle identifying the displayed portion of the image
 *
 * Updates the displayed rectangle (aka viewport) on the image surface to
 * match the specified coordinates and size.
 *
 * \param[in]    img       Image descriptor
 * \param[in]    x         leftmost column
 * \param[in]    y         topmost row
 * \param[in]    w         width
 * \param[in]    h         height
 *
 * \return 0 if the requested rectangle is valid, nonzero otherwise.
 */
int aom_img_set_rect(aom_image_t *img, unsigned int x, unsigned int y,
                     unsigned int w, unsigned int h);

/*!\brief Flip the image vertically (top for bottom)
 *
 * Adjusts the image descriptor's pointers and strides to make the image
 * be referenced upside-down.
 *
 * \param[in]    img       Image descriptor
 */
void aom_img_flip(aom_image_t *img);

/*!\brief Close an image descriptor
 *
 * Frees all allocated storage associated with an image descriptor.
 *
 * \param[in]    img       Image descriptor
 */
void aom_img_free(aom_image_t *img);

/*!\brief Get the width of a plane
 *
 * Get the width of a plane of an image
 *
 * \param[in]    img       Image descriptor
 * \param[in]    plane     Plane index
 */
int aom_img_plane_width(const aom_image_t *img, int plane);

/*!\brief Get the height of a plane
 *
 * Get the height of a plane of an image
 *
 * \param[in]    img       Image descriptor
 * \param[in]    plane     Plane index
 */
int aom_img_plane_height(const aom_image_t *img, int plane);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_IMAGE_H_
