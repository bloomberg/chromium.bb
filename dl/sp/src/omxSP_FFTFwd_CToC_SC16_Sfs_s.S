;//
;// 
;// File Name:  omxSP_FFTFwd_CToC_SC16_Sfs_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   6729
;// Last Modified Date:       Tue, 17 Jul 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute an inverse FFT for a complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
        
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe 
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe 
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix8_fs_OutOfPlace_unsafe 
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe  
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix8_fs_OutOfPlace_unsafe 
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix2_OutOfPlace_unsafe  
        IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe 
        IMPORT  armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe 
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}



;// Guarding implementation by the processor name
    
    

;// Guarding implementation by the processor name
    
    IF  CortexA8
    
    IMPORT  armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe 
    IMPORT  armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe 
    
;//Input Registers

pSrc            RN  0
pDst            RN  1
pFFTSpec        RN  2
scale           RN  3


;// Output registers
result          RN  0

;//Local Scratch Registers

argTwiddle      RN  1
argDst          RN  2
argScale        RN  4
pTwiddle        RN  4
tmpOrder        RN  4
pOut            RN  5
subFFTSize      RN  7     
subFFTNum       RN  6
N               RN  6
order           RN  14
diff            RN  9
count           RN  8                   ;// Total num of radix stages required to comple the FFT
x0r             RN  4    
x0i             RN  5
diffMinusOne    RN  2
round           RN  3

;// Neon registers

dX0             DN  D0.S16
dShift          DN  D1.S16
dX0S32          DN  D0.S32



    ;// Allocate stack memory required by the function
        M_ALLOC4        diffOnStack, 4

    ;// Write function header
        M_START     omxSP_FFTFwd_CToC_SC16_Sfs,r11,d15
        
        M_STRUCT     ARMsFFTSpec
        M_FIELD      N, 4
        M_FIELD      pBitRev, 4
        M_FIELD      pTwiddle, 4
        M_FIELD      pBuf, 4
        M_ENDSTRUCT
        
        ;// Define stack arguments
        
        ;// Read the size from structure and take log
        LDR     N, [pFFTSpec, #ARMsFFTSpec_N]
        
        ;// Read other structure parameters
        LDR     pTwiddle, [pFFTSpec, #ARMsFFTSpec_pTwiddle]
        LDR     pOut, [pFFTSpec, #ARMsFFTSpec_pBuf]
                
        CLZ     order,N                             ;// N = 2^order 
        RSB     order,order,#31     
        MOV     subFFTSize,#1
        ;//MOV     subFFTNum,N
        
        CMP     order,#3
        BGT     orderGreaterthan3                   ;// order > 3
        
        CMP     order,#1
        BGE     orderGreaterthan0                   ;// order > 0
        M_STR   scale, diffOnStack,LT               ;// order = 0
        LDRLT   x0r,[pSrc]
        STRLT   x0r,[pDst]
        MOVLT   pSrc,pDst
        BLT     FFTEnd
        
orderGreaterthan0
        ;// set the buffers appropriately for various orders
        CMP     order,#2
        MOVNE   argDst,pDst        
        MOVEQ   argDst,pOut
        MOVEQ   pOut,pDst                           ;// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle
        
        SUBS     diff,scale,order
        M_STR   diff,diffOnStack
        MOVGT   scale,order
        ;// Now scale <= order
        
        CMP     order,#1
        BGT     orderGreaterthan1
        SUBS    scale,scale,#1
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe  ;// order = 1
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe      ;// order = 1
        B       FFTEnd

orderGreaterthan1
        CMP     order,#2
        MOV     argScale,scale
        BGT     orderGreaterthan2
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe      ;// order =2          
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe  
        SUBS    argScale,argScale,#1
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe  
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe  
        B       FFTEnd
        
orderGreaterthan2                                                               ;// order =3        
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe      
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe  
        SUBS    argScale,argScale,#1
        BLGE    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe  
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ls_OutOfPlace_unsafe      
        BLLT    armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe    
        B       FFTEnd
        

orderGreaterthan3       
        ;// check scale = 0 or scale = order
        SUBS    diff, scale, order                 ;// scale > order 
        MOVGT   scale,order     
        BGE     specialScaleCase                   ;// scale = 0 or scale = order 
        CMP     scale,#0
        BEQ     specialScaleCase
        B       generalScaleCase
        
specialScaleCase                                    ;//  scale = 0 or scale = order  and order > 3     
        
        TST     order, #2                           ;// Set input args to fft stages
        MOVNE   argDst,pDst        
        MOVEQ   argDst,pOut
        MOVEQ   pOut,pDst                           ;// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle  

        CMP      diff,#0
        M_STR    diff, diffOnStack
        BGE      scaleEqualsOrder  
       
        ;//check for even or odd order
        ;// NOTE: The following combination of BL's would work fine eventhough the first
        ;// BL would corrupt the flags. This is because the end of the "grpZeroSetLoop" loop inside 
        ;// armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe sets the Z flag to EQ
        
        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_SC16_Radix4_fs_OutOfPlace_unsafe 
        BLNE    armSP_FFTFwd_CToC_SC16_Radix8_fs_OutOfPlace_unsafe 
        
        CMP        subFFTNum,#4
        BLT     FFTEnd

unscaledRadix4Loop
        BEQ        lastStageUnscaledRadix4
        BL        armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        unscaledRadix4Loop

lastStageUnscaledRadix4
        BL      armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe 
        B        FFTEnd         

scaleEqualsOrder         
        ;//check for even or odd order
        ;// NOTE: The following combination of BL's would work fine eventhough the first
        ;// BL would corrupt the flags. This is because the end of the "grpZeroSetLoop" loop inside 
        ;// armSP_FFTFwd_CToC_SC32_Radix4_fs_OutOfPlace_unsafe sets the Z flag to EQ
                
        TST     order,#0x00000001
        BLEQ    armSP_FFTFwd_CToC_SC16_Sfs_Radix4_fs_OutOfPlace_unsafe 
        BLNE    armSP_FFTFwd_CToC_SC16_Sfs_Radix8_fs_OutOfPlace_unsafe 
        
        CMP        subFFTNum,#4
        BLT     FFTEnd

scaledRadix4Loop
        BEQ        lastStageScaledRadix4
        BL        armSP_FFTFwd_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe
         CMP        subFFTNum,#4
         B        scaledRadix4Loop
         
lastStageScaledRadix4
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe 
        B        FFTEnd                    
         
        
        
generalScaleCase                                        ;// 0 < scale < order and order > 3
        ;// Determine the correct destination buffer
        SUB     diff,order,scale
        TST     diff,#0x01
        ADDEQ   count,scale,diff,LSR #1         ;// count = scale + (order - scale)/2
        MOVNE   count,order
        TST     count,#0x01                     ;// Is count even or odd ?
        
        MOVNE   argDst,pDst                     ;// Set input args to fft stages
        MOVEQ   argDst,pOut
        MOVEQ   pOut,pDst                       ;// Pass the first stage destination in RN5
        MOV     argTwiddle,pTwiddle  

        CMP     diff,#1
        M_STR   diff, diffOnStack    
        BEQ     scaleps                         ;// scaling including a radix2_ps stage
        
        MOV     argScale,scale                  ;// Put scale in RN4 so as to save and restore
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe     ;// scaled first stage
        SUBS    argScale,argScale,#1
        
scaledRadix2Loop        
        BLGT    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1            ;// save and restore scale (RN4) in the scaled stages
        BGT     scaledRadix2Loop
        B       outScale

scaleps
        SUB     argScale,scale,#1                   ;// order>3 and diff=1 => scale >= 3
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe     ;// scaled first stage
        SUBS    argScale,argScale,#1
        
scaledRadix2psLoop
        BEQ     scaledRadix2psStage        
        BLGT    armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe
        SUBS    argScale,argScale,#1            ;// save and restore scale (RN4) in the scaled stages
        BGE     scaledRadix2psLoop

scaledRadix2psStage
        BL      armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe
        B       generalLastStageUnscaledRadix2         
        
        
outScale        
        M_LDR   diff, diffOnStack  
        ;//check for even or odd order
        TST     diff,#0x00000001
        BEQ     generalUnscaledRadix4Loop
        B       unscaledRadix2Loop

generalUnscaledRadix4Loop
        CMP        subFFTNum,#4
         BEQ        generalLastStageUnscaledRadix4
         BL        armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe
         B        generalUnscaledRadix4Loop
         
generalLastStageUnscaledRadix4
        BL      armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe 
        B        End              

unscaledRadix2Loop
        CMP        subFFTNum,#4
         BEQ        generalLastTwoStagesUnscaledRadix2
         BL        armSP_FFTFwd_CToC_SC16_Radix2_OutOfPlace_unsafe
         B        unscaledRadix2Loop        

generalLastTwoStagesUnscaledRadix2
        BL      armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe
generalLastStageUnscaledRadix2                  
        BL      armSP_FFTFwd_CToC_SC16_Radix2_ls_OutOfPlace_unsafe 
        B        End


FFTEnd                                              ;// Does only the scaling
        
        M_LDR   diff, diffOnStack  
        CMP     diff,#0
        BLE     End
        
        RSB     diff,diff,#0                        ;// to use VRSHL for right shift by a variable
        VDUP    dShift,diff     
        
scaleFFTData                                        ;// N = subFFTSize  ; dataptr = pDst  ; scale = diff
        VLD1    {dX0S32[0]},[pSrc]                        ;// pSrc contains pDst pointer
        SUBS    subFFTSize,subFFTSize,#1
        VRSHL   dX0,dShift
        VST1    {dX0S32[0]},[pSrc]!
                
        BGT     scaleFFTData
        
                
       
End                        
        ;// Set return value
        MOV     result, #OMX_Sts_NoErr       

        ;// Write function tail
        M_END
        
    ENDIF                                           ;//CortexA8    

    
    
    
    
    END