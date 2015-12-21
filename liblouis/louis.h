/* liblouis Braille Translation and Back-Translation Library

   Based on the Linux screenreader BRLTTY, copyright (C) 1999-2006 by
   The BRLTTY Team

   Copyright (C) 2004, 2005, 2006
   ViewPlus Technologies, Inc. www.viewplus.com
   and
   JJB Software, Inc. www.jjb-software.com
   All rights reserved

   This file is free software; you can redistribute it and/or modify it
   under the terms of the Lesser or Library GNU General Public License 
   as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version.

   This file is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
   Library GNU General Public License for more details.

   You should have received a copy of the Library GNU General Public 
   License along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

   */

#ifndef __LOUIS_H_
#define __LOUIS_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#include "liblouis.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#define NUMSWAPS 50
#define NUMVAR 50
#define LETSIGNSIZE 128
#define SEQPATTERNSIZE 128
#define CHARSIZE sizeof(widechar)
#define DEFAULTRULESIZE 50
#define ENDSEGMENT 0xffff

/*Definitions of braille dots*/
#define B1 0X01
#define B2 0X02
#define B3 0X04
#define B4 0X08
#define B5 0X10
#define B6 0X20
#define B7 0X40
#define B8 0X80
#define B9 0X100
#define B10 0X200
#define B11 0X400
#define B12 0X800
#define B13 0X1000
#define B14 0X2000
#define B15 0X4000
#define B16 0X8000

/*HASHNUM must be prime */
#define HASHNUM 1123

#define MAXSTRING 2048

  typedef unsigned int TranslationTableOffset;
#define OFFSETSIZE sizeof (TranslationTableOffset)

  typedef enum
  {
    CTC_Space = 0x1,
    CTC_Letter = 0x2,
    CTC_Digit = 0x4,
    CTC_Punctuation = 0x8,
    CTC_UpperCase = 0x10,
    CTC_LowerCase = 0x20,
    CTC_Math = 0x40,
    CTC_Sign = 0x80,
    CTC_LitDigit = 0x100,
    CTC_Class1 = 0x200,
    CTC_Class2 = 0x400,
    CTC_Class3 = 0x800,
    CTC_Class4 = 0x1000,
    CTC_SeqDelimiter = 0x2000,
    CTC_SeqBefore = 0x4000,
    CTC_SeqAfter = 0x8000,
    CTC_UserDefined0 = 0x10000,
    CTC_UserDefined1 = 0x20000,
    CTC_UserDefined2 = 0x40000,
    CTC_UserDefined3 = 0x80000,
    CTC_UserDefined4 = 0x100000,
    CTC_UserDefined5 = 0x200000,
    CTC_UserDefined6 = 0x400000,
    CTC_UserDefined7 = 0x800000,
    CTC_CapsMode = 0x1000000,
    // CTC_EmphMode = 0x2000000,
    CTC_NumericMode = 0x2000000,
    CTC_NumericNoContract = 0x4000000,
    CTC_EndOfInput = 0x8000000   //   used by pattern matcher
  } TranslationTableCharacterAttribute;

  typedef enum
  {
    pass_first = '`',
    pass_last = '~',
    pass_lookback = '_',
    pass_string = '\"',
    pass_dots = '@',
    pass_omit = '?',
    pass_startReplace = '[',
    pass_endReplace = ']',
    pass_startGroup = '{',
    pass_endGroup = '}',
    pass_variable = '#',
    pass_not = '!',
    pass_search = '/',
    pass_any = 'a',
    pass_digit = 'd',
    pass_litDigit = 'D',
    pass_letter = 'l',
    pass_math = 'm',
    pass_punctuation = 'p',
    pass_sign = 'S',
    pass_space = 's',
    pass_uppercase = 'U',
    pass_lowercase = 'u',
    pass_class1 = 'w',
    pass_class2 = 'x',
    pass_class3 = 'y',
    pass_class4 = 'z',
    pass_attributes = '$',
    pass_groupstart = '{',
    pass_groupend = '}',
    pass_groupreplace = ';',
    pass_swap = '%',
    pass_hyphen = '-',
    pass_until = '.',
    pass_eq = '=',
    pass_lt = '<',
    pass_gt = '>',
    pass_endTest = 32,
    pass_plus = '+',
    pass_copy = '*',
    pass_leftParen = '(',
    pass_rightParen = ')',
    pass_comma = ',',
    pass_lteq = 130,
    pass_gteq = 131,
    pass_invalidToken = 132,
    pass_noteq = 133,
    pass_and = 134,
    pass_or = 135,
    pass_nameFound = 136,
    pass_numberFound = 137,
    pass_boolean = 138,
    pass_class = 139,
    pass_define = 140,
    pass_emphasis = 141,
    pass_group = 142,
    pass_mark = 143,
    pass_repGroup = 143,
    pass_script = 144,
    pass_noMoreTokens = 145,
    pass_replace = 146,
    pass_if = 147,
    pass_then = 148,
    pass_all = 255
  }
  pass_Codes;

  typedef unsigned int TranslationTableCharacterAttributes;

  typedef struct
  {
    TranslationTableOffset next;
    widechar lookFor;
    widechar found;
  } CharOrDots;

  typedef struct
  {
    TranslationTableOffset next;
    TranslationTableOffset definitionRule;
    TranslationTableOffset otherRules;
    TranslationTableCharacterAttributes attributes;
    widechar realchar;
    widechar uppercase;
    widechar lowercase;
#if UNICODEBITS == 16
    widechar padding;
#endif
  } TranslationTableCharacter;

  typedef enum
  {				/*Op codes */
    CTO_IncludeFile,
    CTO_Locale,			/*Deprecated, do not use */
    CTO_Undefined,
    CTO_CapitalSign,
    CTO_BeginCapitalSign,
    CTO_LenBegcaps,
    CTO_EndCapitalSign,
    CTO_FirstWordCaps,
    CTO_LastWordCapsBefore,
    CTO_LastWordCapsAfter,
    CTO_LenCapsPhrase,
    CTO_FirstLetterCaps,
    CTO_LastLetterCaps,
    CTO_SingleLetterCaps,
    CTO_CapsWord,
    CTO_CapsWordStop,
    CTO_LetterSign,
    CTO_NoLetsignBefore,
    CTO_NoLetsign,
    CTO_NoLetsignAfter,
    CTO_NumberSign,
    // CTO_NumericModeChars,
    // CTO_NumericNoContractChars,
    CTO_SeqDelimiter,
    CTO_SeqBeforeChars,
    CTO_SeqAfterChars,
    CTO_SeqAfterPattern,
    CTO_ItalSign,
    CTO_BegItal,
    CTO_EndItal,
    CTO_BoldSign,
    CTO_BegBold,
    CTO_EndBold,
    CTO_UnderSign,
    CTO_BegUnder,
    CTO_EndUnder,
    CTO_EmphClass,
    
    /* the following 9 opcodes are compiled further down to one of the 10 x 9 values defined below
       (CTO_SingleLetterItal...CTO_LenTransNotePhrase) */
    CTO_SingleLetterEmph,
    CTO_EmphWord,
    CTO_EmphWordStop,
    CTO_FirstLetterEmph,
    CTO_LastLetterEmph,
    CTO_FirstWordEmph,
    CTO_LastWordEmphBefore,
    CTO_LastWordEmphAfter,
    CTO_LenEmphPhrase,
    
    CTO_CapsModeChars,
    // CTO_EmphModeChars,
    CTO_BegComp,
    CTO_CompBegEmph1,
    CTO_CompEndEmph1,
    CTO_CompBegEmph2,
    CTO_CompEndEmph2,
    CTO_CompBegEmph3,
    CTO_CompEndEmph3,
    CTO_CompCapSign,
    CTO_CompBegCaps,
    CTO_CompEndCaps,
    CTO_EndComp,
    CTO_MultInd,
    CTO_CompDots,
    CTO_Comp6,
    CTO_Class,			/*define a character class */
    CTO_After,			/*only match if after character in class */
    CTO_Before,			/*only match if before character in class               30      */
    CTO_NoBack,
    CTO_NoFor,
    CTO_SwapCc,
    CTO_SwapCd,
    CTO_SwapDd,
    CTO_Space,
    CTO_Digit,
    CTO_Punctuation,
    CTO_Math,
    CTO_Sign,
    CTO_Letter,
    CTO_UpperCase,
    CTO_LowerCase,
    CTO_Grouping,
    CTO_UpLow,
    CTO_LitDigit,
    CTO_Display,
    CTO_Replace,
    CTO_Context,
    CTO_Correct,
    CTO_Pass2,
    CTO_Pass3,
    CTO_Pass4,
    CTO_Repeated,
    CTO_RepWord,
    CTO_CapsNoCont,
    CTO_Always,
    CTO_ExactDots,
    CTO_NoCross,
    CTO_Syllable,
    CTO_NoContractSign,
    CTO_NoCont,
    CTO_CompBrl,
    CTO_Literal,
    CTO_LargeSign,
    CTO_WholeWord,
    CTO_PartWord,
    CTO_JoinNum,
    CTO_JoinableWord,
    CTO_LowWord,
    CTO_Contraction,
    CTO_SuffixableWord,		/*whole word or beginning of word */
    CTO_PrefixableWord,		/*whole word or end of word */
    CTO_BegWord,		/*beginning of word only */
    CTO_BegMidWord,		/*beginning or middle of word */
    CTO_MidWord,		/*middle of word only                                                                           20      */
    CTO_MidEndWord,		/*middle or end of word */
    CTO_EndWord,		/*end of word only */
    CTO_PrePunc,		/*punctuation in string at beginning of word */
    CTO_PostPunc,		/*punctuation in string at end of word */
    CTO_BegNum,			/*beginning of number */
    CTO_MidNum,			/*middle of number, e.g., decimal point */
    CTO_EndNum,			/*end of number */
    CTO_DecPoint,
    CTO_Hyphen,
    //CTO_Apostrophe,
    //CTO_Initial,
    CTO_NoBreak,
    CTO_Match,
    CTO_Attribute,
    CTO_None,
    
    /* Start of (10 x 9) internal opcodes values that match {"singleletteremph"..."lenemphphrase"}
     */
    CTO_SingleLetterItal,        // singleletteremph {emph_1}
    CTO_ItalWord,                // emphword {emph_1}
    CTO_ItalWordStop,            // emphwordstop {emph_1}
    CTO_FirstLetterItal,         // firstletteremph {emph_1}
    CTO_LastLetterItal,          // lastletteremph {emph_1}
    CTO_FirstWordItal,           // firstwordemph {emph_1}
    CTO_LastWordItalBefore,      // lastwordemphbefore {emph_1}
    CTO_LastWordItalAfter,       // lastwordemphafter {emph_1}
    CTO_LenItalPhrase,           // lenemphphrase {emph_1}
    
    CTO_SingleLetterUnder,       // singleletteremph {emph_2}
    CTO_UnderWord,               // emphword {emph_2}
    CTO_UnderWordStop,           // emphwordstop {emph_2}
    CTO_FirstLetterUnder,        // firstletteremph {emph_2}
    CTO_LastLetterUnder,         // lastletteremph {emph_2}
    CTO_FirstWordUnder,          // firstwordemph {emph_2}
    CTO_LastWordUnderBefore,     // lastwordemphbefore {emph_2}
    CTO_LastWordUnderAfter,      // lastwordemphafter {emph_2}
    CTO_LenUnderPhrase,          // lenemphphrase {emph_2}
    
    CTO_SingleLetterBold,        // singleletteremph {emph_3}
    CTO_BoldWord,                // emphword {emph_3}
    CTO_BoldWordStop,            // emphwordstop {emph_3}
    CTO_FirstLetterBold,         // firstletteremph {emph_3}
    CTO_LastLetterBold,          // lastletteremph {emph_3}
    CTO_FirstWordBold,           // firstwordemph {emph_3}
    CTO_LastWordBoldBefore,      // lastwordemphbefore {emph_3}
    CTO_LastWordBoldAfter,       // lastwordemphafter {emph_3}
    CTO_LenBoldPhrase,           // lenemphphrase {emph_3}
    
    CTO_SingleLetterScript,      // singleletteremph {emph_4}
    CTO_ScriptWord,              // emphword {emph_4}
    CTO_ScriptWordStop,          // emphwordstop {emph_4}
    CTO_FirstLetterScript,       // firstletteremph {emph_4}
    CTO_LastLetterScript,        // lastletteremph {emph_4}
    CTO_FirstWordScript,         // firstwordemph {emph_4}
    CTO_LastWordScriptBefore,    // lastwordemphbefore {emph_4}
    CTO_LastWordScriptAfter,     // lastwordemphafter {emph_4}
    CTO_LenScriptPhrase,         // lenemphphrase {emph_4}
    
    CTO_SingleLetterTransNote,   // singleletteremph {emph_5}
    CTO_TransNoteWord,           // emphword {emph_5}
    CTO_TransNoteWordStop,       // emphwordstop {emph_5}
    CTO_FirstLetterTransNote,    // firstletteremph {emph_5}
    CTO_LastLetterTransNote,     // lastletteremph {emph_5}
    CTO_FirstWordTransNote,      // firstwordemph {emph_5}
    CTO_LastWordTransNoteBefore, // lastwordemphbefore {emph_5}
    CTO_LastWordTransNoteAfter,  // lastwordemphafter {emph_5}
    CTO_LenTransNotePhrase,      // lenemphphrase {emph_5}
    
    CTO_SingleLetterTrans1,      // singleletteremph {emph_6}
    CTO_Trans1Word,              // emphword {emph_6}
    CTO_Trans1WordStop,          // emphwordstop {emph_6}
    CTO_FirstLetterTrans1,       // firstletteremph {emph_6}
    CTO_LastLetterTrans1,        // lastletteremph {emph_6}
    CTO_FirstWordTrans1,         // firstwordemph {emph_6}
    CTO_LastWordTrans1Before,    // lastwordemphbefore {emph_6}
    CTO_LastWordTrans1After,     // lastwordemphafter {emph_6}
    CTO_LenTrans1Phrase,         // lenemphphrase {emph_6}
    
    CTO_SingleLetterTrans2,      // singleletteremph {emph_7}
    CTO_Trans2Word,              // emphword {emph_7}
    CTO_Trans2WordStop,          // emphwordstop {emph_7}
    CTO_FirstLetterTrans2,       // firstletteremph {emph_7}
    CTO_LastLetterTrans2,        // lastletteremph {emph_7}
    CTO_FirstWordTrans2,         // firstwordemph {emph_7}
    CTO_LastWordTrans2Before,    // lastwordemphbefore {emph_7}
    CTO_LastWordTrans2After,     // lastwordemphafter {emph_7}
    CTO_LenTrans2Phrase,         // lenemphphrase {emph_7}
    
    CTO_SingleLetterTrans3,      // singleletteremph {emph_8}
    CTO_Trans3Word,              // emphword {emph_8}
    CTO_Trans3WordStop,          // emphwordstop {emph_8}
    CTO_FirstLetterTrans3,       // firstletteremph {emph_8}
    CTO_LastLetterTrans3,        // lastletteremph {emph_8}
    CTO_FirstWordTrans3,         // firstwordemph {emph_8}
    CTO_LastWordTrans3Before,    // lastwordemphbefore {emph_8}
    CTO_LastWordTrans3After,     // lastwordemphafter {emph_8}
    CTO_LenTrans3Phrase,         // lenemphphrase {emph_8}
    
    CTO_SingleLetterTrans4,      // singleletteremph {emph_9}
    CTO_Trans4Word,              // emphword {emph_9}
    CTO_Trans4WordStop,          // emphwordstop {emph_9}
    CTO_FirstLetterTrans4,       // firstletteremph {emph_9}
    CTO_LastLetterTrans4,        // lastletteremph {emph_9}
    CTO_FirstWordTrans4,         // firstwordemph {emph_9}
    CTO_LastWordTrans4Before,    // lastwordemphbefore {emph_9}
    CTO_LastWordTrans4After,     // lastwordemphafter {emph_9}
    CTO_LenTrans4Phrase,         // lenemphphrase {emph_9}
    
    CTO_SingleLetterTrans5,      // singleletteremph {emph_10}
    CTO_Trans5Word,              // emphword {emph_10}
    CTO_Trans5WordStop,          // emphwordstop {emph_10}
    CTO_FirstLetterTrans5,       // firstletteremph {emph_10}
    CTO_LastLetterTrans5,        // lastletteremph {emph_10}
    CTO_FirstWordTrans5,         // firstwordemph {emph_10}
    CTO_LastWordTrans5Before,    // lastwordemphbefore {emph_10}
    CTO_LastWordTrans5After,     // lastwordemphafter {emph_10}
    CTO_LenTrans5Phrase,         // lenemphphrase {emph_10}
    
    /* More internal opcodes */
    CTO_CapitalRule,
    CTO_BeginCapitalRule,
    CTO_EndCapitalRule,
    CTO_FirstWordCapsRule,
    CTO_LastWordCapsBeforeRule,
    CTO_LastWordCapsAfterRule,
    CTO_CapsWordRule,
    CTO_CapsWordStopRule,
    CTO_LetterRule,
    CTO_NumberRule,
    CTO_NoContractRule,
    CTO_FirstWordItalRule,
    CTO_LastWordItalBeforeRule,
    CTO_LastWordItalAfterRule,
    CTO_FirstLetterItalRule,
    CTO_LastLetterItalRule,
    CTO_SingleLetterItalRule,
    CTO_ItalWordRule,
    CTO_ItalWordStopRule,
    CTO_FirstWordBoldRule,
    CTO_LastWordBoldBeforeRule,
    CTO_LastWordBoldAfterRule,
    CTO_FirstLetterBoldRule,
    CTO_LastLetterBoldRule,
    CTO_SingleLetterBoldRule,
    CTO_BoldWordRule,
    CTO_BoldWordStopRule,
    CTO_FirstWordUnderRule,
    CTO_LastWordUnderBeforeRule,
    CTO_LastWordUnderAfterRule,
    CTO_FirstLetterUnderRule,
    CTO_LastLetterUnderRule,
    CTO_SingleLetterUnderRule,
    CTO_UnderWordRule,
    CTO_UnderWordStopRule,
    CTO_SingleLetterScriptRule,
    CTO_ScriptWordRule,
    CTO_ScriptWordStopRule,
    CTO_FirstLetterScriptRule,
    CTO_LastLetterScriptRule,
    CTO_FirstWordScriptRule,
    CTO_LastWordScriptBeforeRule,
    CTO_LastWordScriptAfterRule,
    CTO_LenScriptPhraseRule,
    CTO_SingleLetterTrans1Rule,
    CTO_Trans1WordRule,
    CTO_Trans1WordStopRule,
    CTO_FirstLetterTrans1Rule,
    CTO_LastLetterTrans1Rule,
    CTO_FirstWordTrans1Rule,
    CTO_LastWordTrans1BeforeRule,
    CTO_LastWordTrans1AfterRule,
    CTO_LenTrans1PhraseRule,
    CTO_SingleLetterTrans2Rule,
    CTO_Trans2WordRule,
    CTO_Trans2WordStopRule,
    CTO_FirstLetterTrans2Rule,
    CTO_LastLetterTrans2Rule,
    CTO_FirstWordTrans2Rule,
    CTO_LastWordTrans2BeforeRule,
    CTO_LastWordTrans2AfterRule,
    CTO_LenTrans2PhraseRule,
    CTO_SingleLetterTrans3Rule,
    CTO_Trans3WordRule,
    CTO_Trans3WordStopRule,
    CTO_FirstLetterTrans3Rule,
    CTO_LastLetterTrans3Rule,
    CTO_FirstWordTrans3Rule,
    CTO_LastWordTrans3BeforeRule,
    CTO_LastWordTrans3AfterRule,
    CTO_LenTrans3PhraseRule,
    CTO_SingleLetterTrans4Rule,
    CTO_Trans4WordRule,
    CTO_Trans4WordStopRule,
    CTO_FirstLetterTrans4Rule,
    CTO_LastLetterTrans4Rule,
    CTO_FirstWordTrans4Rule,
    CTO_LastWordTrans4BeforeRule,
    CTO_LastWordTrans4AfterRule,
    CTO_LenTrans4PhraseRule,
    CTO_SingleLetterTrans5Rule,
    CTO_Trans5WordRule,
    CTO_Trans5WordStopRule,
    CTO_FirstLetterTrans5Rule,
    CTO_LastLetterTrans5Rule,
    CTO_FirstWordTrans5Rule,
    CTO_LastWordTrans5BeforeRule,
    CTO_LastWordTrans5AfterRule,
    CTO_LenTrans5PhraseRule,
    CTO_SingleLetterTransNoteRule,
    CTO_TransNoteWordRule,
    CTO_TransNoteWordStopRule,
    CTO_FirstLetterTransNoteRule,
    CTO_LastLetterTransNoteRule,
    CTO_FirstWordTransNoteRule,
    CTO_LastWordTransNoteBeforeRule,
    CTO_LastWordTransNoteAfterRule,
    CTO_LenTransNotePhraseRule,
    CTO_BegCompRule,
    CTO_CompBegEmph1Rule,
    CTO_CompEndEmph1Rule,
    CTO_CompBegEmph2Rule,
    CTO_CompEndEmrh2Rule,
    CTO_CompBegEmph3Rule,
    CTO_CompEndEmph3Rule,
    CTO_CompCapSignRule,
    CTO_CompBegCapsRule,
    CTO_CompEndCapsRule,
    CTO_EndCompRule,
    CTO_CapsNoContRule,
    CTO_All
  } TranslationTableOpcode;

  typedef struct
  {
    TranslationTableOffset charsnext;	/*next chars entry */
    TranslationTableOffset dotsnext;	/*next dots entry */
    TranslationTableCharacterAttributes after;	/*character types which must foollow */
    TranslationTableCharacterAttributes before;	/*character types which must 
						   precede */
	TranslationTableOffset patterns;   /*   before and after patterns   */
    TranslationTableOpcode opcode;	/*rule for testing validity of replacement */
    short charslen;		/*length of string to be replaced */
    short dotslen;		/*length of replacement string */
    widechar charsdots[DEFAULTRULESIZE];	/*find and replacement 
						   strings */
  } TranslationTableRule;

  typedef struct		/*state transition */
  {
    widechar ch;
    widechar newState;
  } HyphenationTrans;

  typedef union
  {
    HyphenationTrans *pointer;
    TranslationTableOffset offset;
  } PointOff;

  typedef struct		/*one state */
  {
    PointOff trans;
    TranslationTableOffset hyphenPattern;
    widechar fallbackState;
    widechar numTrans;
  } HyphenationState;

  /*Translation table header */
  typedef struct
  {				/*translation table */
    int capsNoCont;
    int numPasses;
    int corrections;
    int syllables;
    int usesSequences;
    // int usesEmphMode;
    TranslationTableOffset tableSize;
    TranslationTableOffset bytesUsed;
    TranslationTableOffset undefined;
    TranslationTableOffset letterSign;
    TranslationTableOffset numberSign;
	TranslationTableOffset noContractSign;
    widechar seqPatterns[SEQPATTERNSIZE];
    int seqPatternsCount;
    /*Do not change the order of the following emphasis rule pointers! 
     */
    TranslationTableOffset firstWordItal;
    TranslationTableOffset lastWordItalBefore;
    TranslationTableOffset lastWordItalAfter;
    TranslationTableOffset firstLetterItal;
    TranslationTableOffset lastLetterItal;
    TranslationTableOffset singleLetterItal;
    TranslationTableOffset italWord;
    TranslationTableOffset italWordStop;
    TranslationTableOffset lenItalPhrase;
    TranslationTableOffset firstWordBold;
    TranslationTableOffset lastWordBoldBefore;
    TranslationTableOffset lastWordBoldAfter;
    TranslationTableOffset firstLetterBold;
    TranslationTableOffset lastLetterBold;
    TranslationTableOffset singleLetterBold;
    TranslationTableOffset boldWord;
    TranslationTableOffset boldWordStop;
    TranslationTableOffset lenBoldPhrase;
    TranslationTableOffset firstWordUnder;
    TranslationTableOffset lastWordUnderBefore;
    TranslationTableOffset lastWordUnderAfter;
    TranslationTableOffset firstLetterUnder;
    TranslationTableOffset lastLetterUnder;
    TranslationTableOffset singleLetterUnder;
    TranslationTableOffset underWord;
    TranslationTableOffset underWordStop;
    TranslationTableOffset lenUnderPhrase;
    TranslationTableOffset firstWordCaps;
    TranslationTableOffset lastWordCapsBefore;
    TranslationTableOffset lastWordCapsAfter;
    TranslationTableOffset beginCapitalSign;
    TranslationTableOffset endCapitalSign;	/*end capitals sign */
    TranslationTableOffset capitalSign;
    TranslationTableOffset CapsWord;
    TranslationTableOffset CapsWordStop;
    TranslationTableOffset lenCapsPhrase;
    TranslationTableOffset firstWordScript;
    TranslationTableOffset lastWordScriptBefore;
    TranslationTableOffset lastWordScriptAfter;
    TranslationTableOffset firstLetterScript;
    TranslationTableOffset lastLetterScript;
    TranslationTableOffset singleLetterScript;
    TranslationTableOffset scriptWord;
    TranslationTableOffset scriptWordStop;
    TranslationTableOffset lenScriptPhrase;
    TranslationTableOffset firstWordTrans1;
    TranslationTableOffset lastWordTrans1Before;
    TranslationTableOffset lastWordTrans1After;
    TranslationTableOffset firstLetterTrans1;
    TranslationTableOffset lastLetterTrans1;
    TranslationTableOffset singleLetterTrans1;
    TranslationTableOffset trans1Word;
    TranslationTableOffset trans1WordStop;
    TranslationTableOffset lenTrans1Phrase;
    TranslationTableOffset firstWordTrans2;
    TranslationTableOffset lastWordTrans2Before;
    TranslationTableOffset lastWordTrans2After;
    TranslationTableOffset firstLetterTrans2;
    TranslationTableOffset lastLetterTrans2;
    TranslationTableOffset singleLetterTrans2;
    TranslationTableOffset trans2Word;
    TranslationTableOffset trans2WordStop;
    TranslationTableOffset lenTrans2Phrase;
    TranslationTableOffset firstWordTrans3;
    TranslationTableOffset lastWordTrans3Before;
    TranslationTableOffset lastWordTrans3After;
    TranslationTableOffset firstLetterTrans3;
    TranslationTableOffset lastLetterTrans3;
    TranslationTableOffset singleLetterTrans3;
    TranslationTableOffset trans3Word;
    TranslationTableOffset trans3WordStop;
    TranslationTableOffset lenTrans3Phrase;
    TranslationTableOffset firstWordTrans4;
    TranslationTableOffset lastWordTrans4Before;
    TranslationTableOffset lastWordTrans4After;
    TranslationTableOffset firstLetterTrans4;
    TranslationTableOffset lastLetterTrans4;
    TranslationTableOffset singleLetterTrans4;
    TranslationTableOffset trans4Word;
    TranslationTableOffset trans4WordStop;
    TranslationTableOffset lenTrans4Phrase;
    TranslationTableOffset firstWordTrans5;
    TranslationTableOffset lastWordTrans5Before;
    TranslationTableOffset lastWordTrans5After;
    TranslationTableOffset firstLetterTrans5;
    TranslationTableOffset lastLetterTrans5;
    TranslationTableOffset singleLetterTrans5;
    TranslationTableOffset trans5Word;
    TranslationTableOffset trans5WordStop;
    TranslationTableOffset lenTrans5Phrase;
    TranslationTableOffset firstWordTransNote;
    TranslationTableOffset lastWordTransNoteBefore;
    TranslationTableOffset lastWordTransNoteAfter;
    TranslationTableOffset firstLetterTransNote;
    TranslationTableOffset lastLetterTransNote;
    TranslationTableOffset singleLetterTransNote;
    TranslationTableOffset transNoteWord;
    TranslationTableOffset transNoteWordStop;
    TranslationTableOffset lenTransNotePhrase;
    /* End of ordered emphasis rule poiinters */
    TranslationTableOffset lenBeginCaps;
    TranslationTableOffset begComp;
    TranslationTableOffset compBegEmph1;
    TranslationTableOffset compEndEmph1;
    TranslationTableOffset compBegEmph2;
    TranslationTableOffset compEndEmph2;
    TranslationTableOffset compBegEmph3;
    TranslationTableOffset compEndEmph3;
    TranslationTableOffset compCapSign;
    TranslationTableOffset compBegCaps;
    TranslationTableOffset compEndCaps;
    TranslationTableOffset endComp;
    TranslationTableOffset hyphenStatesArray;
    widechar noLetsignBefore[LETSIGNSIZE];
    int noLetsignBeforeCount;
    widechar noLetsign[LETSIGNSIZE];
    int noLetsignCount;
    widechar noLetsignAfter[LETSIGNSIZE];
    int noLetsignAfterCount;
    TranslationTableOffset characters[HASHNUM];	/*Character 
						   definitions */
    TranslationTableOffset dots[HASHNUM];	/*Dot definitions */
    TranslationTableOffset charToDots[HASHNUM];
    TranslationTableOffset dotsToChar[HASHNUM];
    TranslationTableOffset compdotsPattern[256];
    TranslationTableOffset swapDefinitions[NUMSWAPS];
    TranslationTableOffset attribOrSwapRules[5];
    TranslationTableOffset forRules[HASHNUM];	/*chains of forward rules */
    TranslationTableOffset backRules[HASHNUM];	/*Chains of backward rules */
    TranslationTableOffset ruleArea[1];	/*Space for storing all 
					   rules and values */
  } TranslationTableHeader;
  typedef enum
  {
    alloc_typebuf,
	alloc_wordBuffer,
	alloc_emphasisBuffer,
	alloc_transNoteBuffer,
    alloc_destSpacing,
    alloc_passbuf1,
    alloc_passbuf2,
    alloc_srcMapping,
    alloc_prevSrcMapping
  } AllocBuf;
  
typedef enum
{
	PTN_LAST,
	
	PTN_END_OF_INPUT,

	PTN_NOT,
	PTN_ZERO_MORE,
	PTN_ONE_MORE,

	PTN_CHARS,

	PTN_ATTRIBUTES,
}
PatternCodes;

/* The following function definitions are hooks into 
* compileTranslationTable.c. Some are used by other library modules. 
* Others are used by tools like lou_allround.c and lou_debug.c. */

  char * getTablePath();
/* Comma separated list of directories to search for tables. */

  char ** resolveTable(const char *tableList, const char *base);
/* Resolve tableList against base. */

  widechar getDotsForChar (widechar c);
/* Returns the single-cell dot pattern corresponding to a character. */

  widechar getCharFromDots (widechar d);
/* Returns the character corresponding to a single-cell dot pattern. */

  void *liblouis_allocMem (AllocBuf buffer, int srcmax, int destmax);
/* used by lou_translateString.c and lou_backTranslateString.c ONLY to 
* allocate memory for internal buffers. */

  void *get_table (const char *name);
/* Checks tables for errors and compiles shem. returns a pointer to the 
* table.  */

  int stringHash (const widechar * c);
/* Hash function for character strings */

  int charHash (widechar c);
/* Hash function for single characters */

  char *showString (widechar const *chars, int length);
/* Returns a string in the same format as the characters operand in 
* opcodes */

  char *showDots (widechar const *dots, int length);
/* Returns a character string in the format of the dots operand */

  char *showAttributes (TranslationTableCharacterAttributes a);
/* Returns a character string where the attributes are indicated by the 
* attribute letters used in multipass opcodes */

  TranslationTableOpcode findOpcodeNumber (const char *tofind);
/* Returns the number of the opcode in the string toFind */

  const char *findOpcodeName (TranslationTableOpcode opcode);
/* Returns the name of the opcode associated with an opcode number*/

  int extParseChars (const char *inString, widechar * outString);
/* Takes a character string and produces a sequence of wide characters. 
* Opposite of showString. 
* Returns the length of the widechar sequence.
*/

  int extParseDots (const char *inString, widechar * outString);
/* Takes a character string and produces a sequence of wide characters 
* containing dot patterns. Opposite of showDots. 
* Returns the length of the widechar sequence.
*/

  int other_translate (const char *trantab, const widechar
		       * inbuf,
		       int *inlen, widechar * outbuf, int *outlen,
		       formtype *typeform, char *spacing, int 
		       *outputPos, int
		       *inputPos, int *cursorPos, int mode);

/*Call wrappers for other translators */
  int other_backTranslate (const char *trantab, const widechar
			   * inbuf,
			   int *inlen, widechar * outbuf, int *outlen,
			   formtype *typeform, char *spacing, int 
			   *outputPos, int
			   *inputPos, int *cursorPos, int mode);
/*Call wrappers for other back-translators.*/

  int other_dotsToChar (const char *trantab, widechar * inbuf,
			widechar * outbuf, int length, int mode);
  int other_charToDots (const char *trantab, const widechar
			* inbuf, widechar * outbuf, int length, int mode);

  int translateWithTracing (const char* tableList, const widechar * inbuf,
			    int* inlen, widechar * outbuf, int* outlen,
			    formtype* typeform, char* spacing, int* outputPos,
			    int* inputPos, int* cursorPos, int mode,
			    const TranslationTableRule **rules, int *rulesLen);

  int backTranslateWithTracing (const char *tableList, const widechar * inbuf,
  				int *inlen, widechar * outbuf,
  				int *outlen, formtype *typeform,
  				char *spacing, int *outputPos,
  				int *inputPos, int *cursorPos, int mode,
  				const TranslationTableRule **rules, int *rulesLen);

  char * getLastTableList();
  void debugHook ();
/* Can be inserted in code to be used as a breakpoint in gdb */
void outOfMemory ();
/* Priknts an out-of-memory message and exits*/

void logWidecharBuf(logLevels level, const char *msg, const widechar *wbuf, int wlen);
/* Helper for logging a widechar buffer */

void logMessage(logLevels level, const char *format, ...);
/* Function for closing loggin file */
void closeLogFile();

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __LOUIS_H */
